/*
 * Snd_fx.cpp
 * -----------
 * Purpose: Processing of pattern commands, song length calculation...
 * Notes  : This needs some heavy refactoring.
 *          I thought of actually adding an effect interface class. Every pattern effect
 *          could then be moved into its own class that inherits from the effect interface.
 *          If effect handling differs severely between module formats, every format would have
 *          its own class for that effect. Then, a call chain of effect classes could be set up
 *          for each format, since effects cannot be processed in the same order in all formats.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Sndfile.h"
#include "MIDIMacroParser.h"
#include "mod_specifications.h"
#ifdef MODPLUG_TRACKER
#include "../mptrack/Moddoc.h"
#endif // MODPLUG_TRACKER
#include "tuning.h"
#include "Tables.h"
#include "modsmp_ctrl.h"  // For updating the loop wraparound data with the invert loop effect
#include "plugins/PlugInterface.h"
#include "OPL.h"
#include "MIDIEvents.h"

OPENMPT_NAMESPACE_BEGIN

// Formats which have 7-bit (0...128) instead of 6-bit (0...64) global volume commands, or which are imported to this range (mostly formats which are converted to IT internally)
#ifdef MODPLUG_TRACKER
static constexpr auto GLOBALVOL_7BIT_FORMATS_EXT = MOD_TYPE_MT2;
#else
static constexpr auto GLOBALVOL_7BIT_FORMATS_EXT = MOD_TYPE_NONE;
#endif // MODPLUG_TRACKER
static constexpr auto GLOBALVOL_7BIT_FORMATS = MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_IMF | MOD_TYPE_J2B | MOD_TYPE_MID | MOD_TYPE_AMS | MOD_TYPE_DBM | MOD_TYPE_PTM | MOD_TYPE_MDL | MOD_TYPE_DTM | GLOBALVOL_7BIT_FORMATS_EXT;


// Compensate frequency slide LUTs depending on whether we are handling periods or frequency - "up" and "down" in function name are seen from frequency perspective.
static uint32 GetLinearSlideDownTable    (const CSoundFile *sndFile, uint32 i) { MPT_ASSERT(i < std::size(LinearSlideDownTable));     return sndFile->m_playBehaviour[kPeriodsAreHertz] ? LinearSlideDownTable[i]     : LinearSlideUpTable[i]; }
static uint32 GetLinearSlideUpTable      (const CSoundFile *sndFile, uint32 i) { MPT_ASSERT(i < std::size(LinearSlideDownTable));     return sndFile->m_playBehaviour[kPeriodsAreHertz] ? LinearSlideUpTable[i]       : LinearSlideDownTable[i]; }
static uint32 GetFineLinearSlideDownTable(const CSoundFile *sndFile, uint32 i) { MPT_ASSERT(i < std::size(FineLinearSlideDownTable)); return sndFile->m_playBehaviour[kPeriodsAreHertz] ? FineLinearSlideDownTable[i] : FineLinearSlideUpTable[i]; }
static uint32 GetFineLinearSlideUpTable  (const CSoundFile *sndFile, uint32 i) { MPT_ASSERT(i < std::size(FineLinearSlideDownTable)); return sndFile->m_playBehaviour[kPeriodsAreHertz] ? FineLinearSlideUpTable[i]   : FineLinearSlideDownTable[i]; }

// Minimum parameter of tempo command that is considered to be a BPM rather than a tempo slide
static constexpr TEMPO GetMinimumTempoParam(MODTYPE modType)
{
	return (modType & (MOD_TYPE_MDL | MOD_TYPE_MED | MOD_TYPE_XM | MOD_TYPE_MOD)) ? TEMPO(1, 0) : TEMPO(32, 0);
}


////////////////////////////////////////////////////////////
// Length


// Memory class for GetLength() code
class GetLengthMemory
{
protected:
	const CSoundFile &sndFile;

public:
	std::unique_ptr<PlayState> state;
	struct ChnSettings
	{
		uint32 ticksToRender = 0;	// When using sample sync, we still need to render this many ticks
		bool incChanged = false;	// When using sample sync, note frequency has changed
		uint8 vol = 0xFF;
	};

	std::vector<ChnSettings> chnSettings;
	double elapsedTime;
	const SEQUENCEINDEX m_sequence;
	static constexpr uint32 IGNORE_CHANNEL = uint32_max;

	GetLengthMemory(const CSoundFile &sf, SEQUENCEINDEX sequence)
		: sndFile{sf}
		, state{std::make_unique<PlayState>(sf.m_PlayState)}
		, m_sequence{sequence}
	{
		Reset();
	}

	void Reset()
	{
		if(state->m_midiMacroEvaluationResults)
			state->m_midiMacroEvaluationResults.emplace();
		elapsedTime = 0.0;
		state->m_lTotalSampleCount = 0;
		state->m_nMusicSpeed = sndFile.Order(m_sequence).GetDefaultSpeed();
		state->m_nMusicTempo = sndFile.Order(m_sequence).GetDefaultTempo();
		state->m_ppqPosFract = 0.0;
		state->m_ppqPosBeat = 0;
		state->m_nGlobalVolume = sndFile.m_nDefaultGlobalVolume;
		state->m_globalScriptState.Initialize(sndFile);
		chnSettings.assign(sndFile.GetNumChannels(), {});
		const auto muteFlag = CSoundFile::GetChannelMuteFlag();
		for(CHANNELINDEX chn = 0; chn < sndFile.GetNumChannels(); chn++)
		{
			state->Chn[chn].Reset(ModChannel::resetTotal, sndFile, chn, muteFlag);
			state->Chn[chn].nOldGlobalVolSlide = 0;
			state->Chn[chn].nOldChnVolSlide = 0;
			state->Chn[chn].nLastNote = NOTE_NONE;
		}
	}

	// Increment playback position of sample and envelopes on a channel
	void RenderChannel(CHANNELINDEX channel, uint32 tickDuration, uint32 portaStart = uint32_max)
	{
		ModChannel &chn = state->Chn[channel];
		uint32 numTicks = chnSettings[channel].ticksToRender;
		if(numTicks == IGNORE_CHANNEL || numTicks == 0 || (!chn.IsSamplePlaying() && !chnSettings[channel].incChanged) || chn.pModSample == nullptr)
		{
			return;
		}

		const SamplePosition loopStart(chn.dwFlags[CHN_LOOP] ? chn.nLoopStart : 0u, 0);
		const SamplePosition sampleEnd(chn.dwFlags[CHN_LOOP] ? chn.nLoopEnd : chn.nLength, 0);
		const SmpLength loopLength = chn.nLoopEnd - chn.nLoopStart;
		const bool itEnvMode = sndFile.m_playBehaviour[kITEnvelopePositionHandling];
		const bool updatePitchEnv = (chn.PitchEnv.flags & (ENV_ENABLED | ENV_FILTER)) == ENV_ENABLED;
		bool stopNote = false;

		SamplePosition inc = chn.increment * tickDuration;
		if(chn.dwFlags[CHN_PINGPONGFLAG]) inc.Negate();

		for(uint32 i = 0; i < numTicks; i++)
		{
			bool updateInc = (chn.PitchEnv.flags & (ENV_ENABLED | ENV_FILTER)) == ENV_ENABLED;
			if(i >= portaStart)
			{
				state->m_nTickCount = i - portaStart;
				chn.isFirstTick = (i == portaStart);
				const ModCommand &m = *sndFile.Patterns[state->m_nPattern].GetpModCommand(state->m_nRow, channel);
				auto command = m.command;
				switch(m.volcmd)
				{
				case VOLCMD_TONEPORTAMENTO:
					{
						const auto [porta, clearEffectCommand] = sndFile.GetVolCmdTonePorta(m, 0);
						sndFile.TonePortamento(*state, channel, porta);
						if(clearEffectCommand)
							command = CMD_NONE;
					}
					break;
				case VOLCMD_PORTAUP:
					sndFile.PortamentoUp(*state, channel, static_cast<ModCommand::PARAM>(m.vol << 2), sndFile.m_playBehaviour[kITVolColFinePortamento]);
					break;
				case VOLCMD_PORTADOWN:
					sndFile.PortamentoDown(*state, channel, static_cast<ModCommand::PARAM>(m.vol << 2), sndFile.m_playBehaviour[kITVolColFinePortamento]);
					break;
				default:
					break;
				}
				switch(command)
				{
				case CMD_TONEPORTAMENTO:
					sndFile.TonePortamento(*state, channel, m.param);
					break;
				case CMD_TONEPORTAVOL:
					sndFile.TonePortamento(*state, channel, 0);
					break;
				case CMD_PORTAMENTOUP:
					if(m.param || !(sndFile.GetType() & MOD_TYPE_MOD))
						sndFile.PortamentoUp(*state, channel, m.param, false);
					break;
				case CMD_PORTAMENTODOWN:
					if(m.param || !(sndFile.GetType() & MOD_TYPE_MOD))
						sndFile.PortamentoDown(*state, channel, m.param, false);
					break;
				case CMD_MODCMDEX:
					if(!(m.param & 0x0F) && !(sndFile.GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2)))
						break;
					if((m.param & 0xF0) == 0x10)
						sndFile.FinePortamentoUp(chn, m.param & 0x0F);
					else if((m.param & 0xF0) == 0x20)
						sndFile.FinePortamentoDown(chn, m.param & 0x0F);
					break;
				case CMD_XFINEPORTAUPDOWN:
					if((m.param & 0xF0) == 0x10)
						sndFile.ExtraFinePortamentoUp(chn, m.param & 0x0F);
					else if((m.param & 0xF0) == 0x20)
						sndFile.ExtraFinePortamentoDown(chn, m.param & 0x0F);
					break;
				case CMD_NOTESLIDEUP:
				case CMD_NOTESLIDEDOWN:
				case CMD_NOTESLIDEUPRETRIG:
				case CMD_NOTESLIDEDOWNRETRIG:
					sndFile.NoteSlide(chn, m.param, command == CMD_NOTESLIDEUP || command == CMD_NOTESLIDEUPRETRIG, command == CMD_NOTESLIDEUPRETRIG || command == CMD_NOTESLIDEDOWNRETRIG);
					break;
				default:
					break;
				}

				if(chn.autoSlide.IsActive(AutoSlideCommand::TonePortamento) && !chn.rowCommand.IsTonePortamento())
					sndFile.TonePortamento(*state, channel, chn.portamentoSlide);
				else if(chn.autoSlide.IsActive(AutoSlideCommand::TonePortamentoWithDuration))
					sndFile.TonePortamentoWithDuration(chn, 0);
				if(chn.autoSlide.IsActive(AutoSlideCommand::PortamentoUp))
					sndFile.PortamentoUp(*state, channel, chn.nOldPortaUp, true);
				else if(chn.autoSlide.IsActive(AutoSlideCommand::PortamentoDown))
					sndFile.PortamentoDown(*state, channel, chn.nOldPortaDown, true);
				else if(chn.autoSlide.IsActive(AutoSlideCommand::FinePortamentoUp))
					sndFile.FinePortamentoUp(chn, chn.nOldFinePortaUpDown);
				else if(chn.autoSlide.IsActive(AutoSlideCommand::FinePortamentoDown))
					sndFile.FinePortamentoDown(chn, chn.nOldFinePortaUpDown);
				if(chn.autoSlide.IsActive(AutoSlideCommand::PortamentoFC))
					sndFile.PortamentoFC(chn);

				updateInc = true;
			}

			int32 period = chn.nPeriod;
			if(itEnvMode) sndFile.IncrementEnvelopePositions(chn);
			if(updatePitchEnv)
			{
				sndFile.ProcessPitchFilterEnvelope(chn, period);
				updateInc = true;
			}
			if(!itEnvMode) sndFile.IncrementEnvelopePositions(chn);
			int vol = 0;
			sndFile.ProcessInstrumentFade(chn, vol);

			if(chn.dwFlags[CHN_ADLIB])
				continue;

			if(updateInc || chnSettings[channel].incChanged)
			{
				if(chn.m_CalculateFreq || chn.m_ReCalculateFreqOnFirstTick)
				{
					chn.RecalcTuningFreq(1, 0, sndFile);
					if(!chn.m_CalculateFreq)
						chn.m_ReCalculateFreqOnFirstTick = false;
					else
						chn.m_CalculateFreq = false;
				}
				chn.increment = sndFile.GetChannelIncrement(chn, period, 0).first;
				chnSettings[channel].incChanged = false;
				inc = chn.increment * tickDuration;
				if(chn.dwFlags[CHN_PINGPONGFLAG]) inc.Negate();
			}

			chn.position += inc;

			if(chn.position >= sampleEnd || (chn.position < loopStart && inc.IsNegative()))
			{
				if(!chn.dwFlags[CHN_LOOP] || !loopLength)
				{
					// Past sample end.
					stopNote = true;
					break;
				}
				// We exceeded the sample loop, go back to loop start.
				if(chn.dwFlags[CHN_PINGPONGLOOP])
				{
					if(chn.position < loopStart)
					{
						chn.position = SamplePosition(chn.nLoopStart + chn.nLoopStart, 0) - chn.position;
						chn.dwFlags.flip(CHN_PINGPONGFLAG);
						inc.Negate();
					}
					SmpLength posInt = chn.position.GetUInt() - chn.nLoopStart;
					SmpLength pingpongLength = loopLength * 2;
					if(sndFile.m_playBehaviour[kITPingPongMode]) pingpongLength--;
					posInt %= pingpongLength;
					bool forward = (posInt < loopLength);
					if(forward)
						chn.position.SetInt(chn.nLoopStart + posInt);
					else
						chn.position.SetInt(chn.nLoopEnd - (posInt - loopLength));
					if(forward == chn.dwFlags[CHN_PINGPONGFLAG])
					{
						chn.dwFlags.flip(CHN_PINGPONGFLAG);
						inc.Negate();
					}
				} else
				{
					SmpLength posInt = chn.position.GetUInt();
					if(posInt >= chn.nLoopEnd + loopLength)
					{
						const SmpLength overshoot = posInt - chn.nLoopEnd;
						posInt -= (overshoot / loopLength) * loopLength;
					}
					while(posInt >= chn.nLoopEnd)
					{
						posInt -= loopLength;
					}
					chn.position.SetInt(posInt);
				}
			}
		}
		state->m_nTickCount = 0;

		if(stopNote)
		{
			chn.Stop();
			chn.nPortamentoDest = 0;
		}
		chnSettings[channel].ticksToRender = 0;
	}

	void GlobalVolSlide(ModChannel &chn, ModCommand::PARAM param, uint32 nonRowTicks)
	{
		if(sndFile.m_SongFlags[SONG_AUTO_GLOBALVOL])
			chn.autoSlide.SetActive(AutoSlideCommand::GlobalVolumeSlide, param != 0);
		if(param)
			chn.nOldGlobalVolSlide = param;
		else
			param = chn.nOldGlobalVolSlide;

		if((param & 0x0F) == 0x0F && (param & 0xF0))
		{
			param >>= 4;
			if(!(sndFile.GetType() & GLOBALVOL_7BIT_FORMATS))
				param <<= 1;
			state->m_nGlobalVolume += param << 1;
		} else if((param & 0xF0) == 0xF0 && (param & 0x0F))
		{
			param = (param & 0x0F) << 1;
			if(!(sndFile.GetType() & GLOBALVOL_7BIT_FORMATS))
				param <<= 1;
			state->m_nGlobalVolume -= param;
		} else if(param & 0xF0)
		{
			param >>= 4;
			param <<= 1;
			if(!(sndFile.GetType() & GLOBALVOL_7BIT_FORMATS))
				param <<= 1;
			state->m_nGlobalVolume += param * nonRowTicks;
		} else
		{
			param = (param & 0x0F) << 1;
			if(!(sndFile.GetType() & GLOBALVOL_7BIT_FORMATS)) param <<= 1;
			state->m_nGlobalVolume -= param * nonRowTicks;
		}
		Limit(state->m_nGlobalVolume, 0, 256);
	}
};


// Get mod length in various cases. Parameters:
// [in]  adjustMode: See enmGetLengthResetMode for possible adjust modes.
// [in]  target: Time or position target which should be reached, or no target to get length of the first sub song. Use GetLengthTarget::StartPos to also specify a position from where the seeking should begin.
// [out] See definition of type GetLengthType for the returned values.
std::vector<GetLengthType> CSoundFile::GetLength(enmGetLengthResetMode adjustMode, GetLengthTarget target)
{
	std::vector<GetLengthType> results;
	GetLengthType retval;

	// Are we trying to reach a certain pattern position?
	const bool hasSearchTarget = target.mode != GetLengthTarget::NoTarget && target.mode != GetLengthTarget::GetAllSubsongs;
	const bool adjustSamplePos = (adjustMode & eAdjustSamplePositions) == eAdjustSamplePositions;

	SEQUENCEINDEX sequence = target.sequence;
	if(sequence >= Order.GetNumSequences()) sequence = Order.GetCurrentSequenceIndex();
	const ModSequence &orderList = Order(sequence);

	GetLengthMemory memory(*this, sequence);
	PlayState &playState = *memory.state;
	// Temporary visited rows vector (so that GetLength() won't interfere with the player code if the module is playing at the same time)
	RowVisitor visitedRows(*this, sequence);
	ROWINDEX allowedPatternLoopComplexity = 32768;

	// If sequence starts with some non-existent patterns, find a better start
	while(target.startOrder < orderList.size() && !orderList.IsValidPat(target.startOrder))
	{
		target.startOrder++;
		target.startRow = 0;
	}
	retval.startRow = playState.m_nNextRow = playState.m_nRow = target.startRow;
	retval.startOrder = playState.m_nNextOrder = playState.m_nCurrentOrder = target.startOrder;

	// Fast LUTs for commands that are too weird / complicated / whatever to emulate in sample position adjust mode.
	std::bitset<MAX_EFFECTS> forbiddenCommands;

	if(adjustSamplePos)
	{
		forbiddenCommands.set(CMD_ARPEGGIO);

		if(target.mode == GetLengthTarget::SeekPosition && target.pos.order < orderList.size())
		{
			// If we know where to seek, we can directly rule out any channels on which a new note would be triggered right at the start.
			const PATTERNINDEX seekPat = orderList[target.pos.order];
			if(Patterns.IsValidPat(seekPat) && Patterns[seekPat].IsValidRow(target.pos.row))
			{
				const ModCommand *m = Patterns[seekPat].GetpModCommand(target.pos.row, 0);
				for(CHANNELINDEX i = 0; i < GetNumChannels(); i++, m++)
				{
					if(m->note == NOTE_NOTECUT || m->note == NOTE_KEYOFF || (m->note == NOTE_FADE && GetNumInstruments())
						|| (m->IsNote() && m->instr && !m->IsTonePortamento()))
					{
						memory.chnSettings[i].ticksToRender = GetLengthMemory::IGNORE_CHANNEL;
					}
				}
			}
		}
	}

	if(adjustMode & eAdjust)
		playState.m_midiMacroEvaluationResults.emplace();

	// If samples are being synced, force them to resync if tick duration changes
	uint32 oldTickDuration = 0;
	bool breakToRow = false;

	for (;;)
	{
		const bool ignoreRow = NextRow(playState, breakToRow).first;

		// Time target reached.
		if(target.mode == GetLengthTarget::SeekSeconds && memory.elapsedTime >= target.time)
		{
			retval.targetReached = true;
			break;
		}

		// Check if pattern is valid
		playState.m_nPattern = playState.m_nCurrentOrder < orderList.size() ? orderList[playState.m_nCurrentOrder] : PATTERNINDEX_INVALID;
		playState.m_nTickCount = 0;

		if(!Patterns.IsValidPat(playState.m_nPattern) && playState.m_nPattern != PATTERNINDEX_INVALID && target.mode == GetLengthTarget::SeekPosition && playState.m_nCurrentOrder == target.pos.order)
		{
			// Early test: Target is inside +++ or non-existing pattern
			retval.targetReached = true;
			break;
		}

		while(playState.m_nPattern >= Patterns.Size())
		{
			// End of song?
			if((playState.m_nPattern == PATTERNINDEX_INVALID) || (playState.m_nCurrentOrder >= orderList.size()))
			{
				if(playState.m_nCurrentOrder == orderList.GetRestartPos())
					break;
				else
					playState.m_nCurrentOrder = orderList.GetRestartPos();
			} else
			{
				playState.m_nCurrentOrder++;
			}
			playState.m_nPattern = (playState.m_nCurrentOrder < orderList.size()) ? orderList[playState.m_nCurrentOrder] : PATTERNINDEX_INVALID;
			playState.m_nNextOrder = playState.m_nCurrentOrder;
			if((!Patterns.IsValidPat(playState.m_nPattern)) && visitedRows.Visit(playState.m_nCurrentOrder, 0, playState.Chn, ignoreRow))
			{
				if(!hasSearchTarget)
				{
					retval.restartOrder = playState.m_nCurrentOrder;
					retval.restartRow = 0;
				}
				if(target.mode == GetLengthTarget::NoTarget || !visitedRows.GetFirstUnvisitedRow(playState.m_nNextOrder, playState.m_nRow, true))
				{
					// We aren't searching for a specific row, or we couldn't find any more unvisited rows.
					break;
				} else
				{
					// We haven't found the target row yet, but we found some other unplayed row... continue searching from here.
					retval.duration = memory.elapsedTime;
					results.push_back(retval);
					retval.startRow = playState.m_nRow;
					retval.startOrder = playState.m_nNextOrder;
					memory.Reset();

					playState.m_nCurrentOrder = playState.m_nNextOrder;
					playState.m_nPattern = orderList[playState.m_nCurrentOrder];
					playState.m_nNextRow = playState.m_nRow;
					break;
				}
			}
		}
		if(playState.m_nNextOrder == ORDERINDEX_INVALID)
		{
			// GetFirstUnvisitedRow failed, so there is nothing more to play
			break;
		}

		// Skip non-existing patterns
		if(!Patterns.IsValidPat(playState.m_nPattern))
		{
			// If there isn't even a tune, we should probably stop here.
			if(playState.m_nCurrentOrder == orderList.GetRestartPos())
			{
				if(target.mode == GetLengthTarget::NoTarget || !visitedRows.GetFirstUnvisitedRow(playState.m_nNextOrder, playState.m_nRow, true))
				{
					// We aren't searching for a specific row, or we couldn't find any more unvisited rows.
					break;
				} else
				{
					// We haven't found the target row yet, but we found some other unplayed row... continue searching from here.
					retval.duration = memory.elapsedTime;
					results.push_back(retval);
					retval.startRow = playState.m_nRow;
					retval.startOrder = playState.m_nNextOrder;
					memory.Reset();
					playState.m_nNextRow = playState.m_nRow;
					continue;
				}
			}
			playState.m_nNextOrder = playState.m_nCurrentOrder + 1;
			continue;
		}
		// Should never happen
		if(playState.m_nRow >= Patterns[playState.m_nPattern].GetNumRows())
			playState.m_nRow = 0;

		// Check whether target was reached.
		if(target.mode == GetLengthTarget::SeekPosition && playState.m_nCurrentOrder == target.pos.order && playState.m_nRow == target.pos.row)
		{
			retval.targetReached = true;
			break;
		}

		// If pattern loops are nested too deeply, they can cause an effectively infinite amount of loop evalations to be generated.
		// As we don't want the user to wait forever, we bail out if the pattern loops are too complex.
		const bool moduleTooComplex = target.mode != GetLengthTarget::SeekSeconds && visitedRows.ModuleTooComplex(allowedPatternLoopComplexity);
		if(moduleTooComplex)
		{
			memory.elapsedTime = std::numeric_limits<decltype(memory.elapsedTime)>::infinity();
			// Decrease allowed complexity with each subsong, as this seems to be a malicious module
			if(allowedPatternLoopComplexity > 256)
				allowedPatternLoopComplexity /= 2;
			visitedRows.ResetComplexity();
		}

		if(visitedRows.Visit(playState.m_nCurrentOrder, playState.m_nRow, playState.Chn, ignoreRow) || moduleTooComplex)
		{
			if(!hasSearchTarget)
			{
				retval.restartOrder = playState.m_nCurrentOrder;
				retval.restartRow = playState.m_nRow;
			}
			if(target.mode == GetLengthTarget::NoTarget || !visitedRows.GetFirstUnvisitedRow(playState.m_nNextOrder, playState.m_nRow, true))
			{
				// We aren't searching for a specific row, or we couldn't find any more unvisited rows.
				break;
			} else
			{
				// We haven't found the target row yet, but we found some other unplayed row... continue searching from here.
				retval.duration = memory.elapsedTime;
				results.push_back(retval);
				retval.startRow = playState.m_nRow;
				retval.startOrder = playState.m_nNextOrder;
				memory.Reset();
				playState.m_nNextRow = playState.m_nRow;
				continue;
			}
		}

		retval.endOrder = playState.m_nCurrentOrder;
		retval.endRow = playState.m_nRow;

		// Update next position
		SetupNextRow(playState, false);

		// Jumped to invalid pattern row?
		if(playState.m_nRow >= Patterns[playState.m_nPattern].GetNumRows())
		{
			playState.m_nRow = 0;
		}

		playState.UpdatePPQ(breakToRow);
		playState.UpdateTimeSignature(*this);

		if(ignoreRow)
			continue;

		// For various effects, we need to know first how many ticks there are in this row.
		const ModCommand *p = Patterns[playState.m_nPattern].GetpModCommand(playState.m_nRow, 0);
		const bool ignoreMutedChn = m_playBehaviour[kST3NoMutedChannels];
		for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); nChn++, p++)
		{
			ModChannel &chn = playState.Chn[nChn];
			chn.isFirstTick = true;
			if(p->IsEmpty() || (ignoreMutedChn && ChnSettings[nChn].dwFlags[CHN_MUTE]))  // not even effects are processed on muted S3M channels
			{
				chn.rowCommand.Clear();
				continue;
			}
			if(p->IsPcNote())
			{
#ifndef NO_PLUGINS
				if(playState.m_midiMacroEvaluationResults && p->instr > 0 && p->instr <= MAX_MIXPLUGINS)
				{
					playState.m_midiMacroEvaluationResults->pluginParameter[{static_cast<PLUGINDEX>(p->instr - 1), p->GetValueVolCol()}] = p->GetValueEffectCol() / PlugParamValue(ModCommand::maxColumnValue);
				}
#endif // NO_PLUGINS
				chn.rowCommand.Clear();
				continue;
			}

			if(p->IsNote())
				chn.nNewNote = chn.nLastNote = p->note;
			else if(p->note > NOTE_MAX && m_playBehaviour[kITClearOldNoteAfterCut])
				chn.nNewNote = NOTE_NONE;

			if(m_playBehaviour[kITEmptyNoteMapSlotIgnoreCell] && p->instr > 0 && p->instr <= GetNumInstruments()
				&& Instruments[p->instr] != nullptr && !Instruments[p->instr]->HasValidMIDIChannel())
			{
				auto note = (chn.rowCommand.note != NOTE_NONE) ? p->note : chn.nNewNote;
				if (ModCommand::IsNote(note) && Instruments[p->instr]->Keyboard[note - NOTE_MIN] == 0)
				{
					chn.nNewNote = chn.nLastNote = note;
					chn.nNewIns = p->instr;
					chn.rowCommand.Clear();
					continue;
				}
			}

			chn.rowCommand = *p;
			switch(p->command)
			{
			case CMD_SPEED:
				SetSpeed(playState, p->param);
				break;

			case CMD_TEMPO:
				if(m_playBehaviour[kMODVBlankTiming])
				{
					// ProTracker MODs with VBlank timing: All Fxx parameters set the tick count.
					if(p->param != 0) SetSpeed(playState, p->param);
				}
				// Regular tempo handled below
				break;

			case CMD_S3MCMDEX:
				if(!chn.rowCommand.param && (GetType() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)))
					chn.rowCommand.param = chn.nOldCmdEx;
				else
					chn.nOldCmdEx = static_cast<ModCommand::PARAM>(chn.rowCommand.param);
				if((p->param & 0xF0) == 0x60)
				{
					// Fine Pattern Delay
					playState.m_nFrameDelay += (p->param & 0x0F);
				} else if((p->param & 0xF0) == 0xE0 && !playState.m_nPatternDelay)
				{
					// Pattern Delay
					if(!(GetType() & MOD_TYPE_S3M) || (p->param & 0x0F) != 0)
					{
						// While Impulse Tracker *does* count S60 as a valid row delay (and thus ignores any other row delay commands on the right),
						// Scream Tracker 3 simply ignores such commands.
						playState.m_nPatternDelay = 1 + (p->param & 0x0F);
					}
				}
				break;

			case CMD_MODCMDEX:
				if((p->param & 0xF0) == 0xE0)
				{
					// Pattern Delay
					playState.m_nPatternDelay = 1 + (p->param & 0x0F);
				}
				break;

			default:
				break;
			}
		}
		// This may change speed/tempo/global volume/next row
		playState.m_globalScriptState.NextTick(playState, *this);

		const uint32 numTicks = playState.TicksOnRow();
		const uint32 nonRowTicks = numTicks - std::max(playState.m_nPatternDelay, uint32(1));

		playState.m_patLoopRow = ROWINDEX_INVALID;
		playState.m_breakRow = ROWINDEX_INVALID;
		playState.m_posJump = ORDERINDEX_INVALID;

		for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); nChn++)
		{
			ModChannel &chn = playState.Chn[nChn];
			if(chn.rowCommand.IsEmpty() && !chn.autoSlide.AnyActive())
				continue;
			ModCommand::COMMAND command = chn.rowCommand.command;
			ModCommand::PARAM param = chn.rowCommand.param;
			ModCommand::NOTE note = chn.rowCommand.note;

			if((adjustMode & eAdjust) && !chn.rowCommand.IsEmpty())
			{
				if(chn.rowCommand.instr)
				{
					chn.swapSampleIndex = chn.nNewIns = chn.rowCommand.instr;
					memory.chnSettings[nChn].vol = 0xFF;
				}
				if(chn.rowCommand.IsNote())
				{
					chn.RestorePanAndFilter();

					if(!adjustSamplePos || memory.chnSettings[nChn].ticksToRender == GetLengthMemory::IGNORE_CHANNEL)
					{
						// Even if we don't intend to render anything on this channel, update instrument cutoff/resonance because it might override a Zxx effect evaluated earlier.
						const ModInstrument *instr = chn.pModInstrument;
						if(chn.nNewIns > 0 && chn.nNewIns <= GetNumInstruments())
							instr = Instruments[chn.nNewIns].get();
						if(instr != nullptr)
						{
							if(instr->IsCutoffEnabled())
								chn.nCutOff = instr->GetCutoff();
							if(instr->IsResonanceEnabled())
								chn.nResonance = instr->GetResonance();
						}
						const bool wasGlobalSlideRunning = chn.autoSlide.IsActive(AutoSlideCommand::GlobalVolumeSlide);
						chn.autoSlide.Reset();
						chn.autoSlide.SetActive(AutoSlideCommand::GlobalVolumeSlide, wasGlobalSlideRunning);
					}
				}

				// Update channel panning
				if(chn.rowCommand.IsNote() || chn.rowCommand.instr)
				{
					ModInstrument *pIns;
					if(chn.nNewIns > 0 && chn.nNewIns <= GetNumInstruments() && (pIns = Instruments[chn.nNewIns].get()) != nullptr)
					{
						if(pIns->dwFlags[INS_SETPANNING])
							chn.SetInstrumentPan(pIns->nPan, *this);
					}
					const SAMPLEINDEX smp = GetSampleIndex(note, chn.nNewIns);
					if(smp > 0)
					{
						if(Samples[smp].uFlags[CHN_PANNING])
							chn.SetInstrumentPan(Samples[smp].nPan, *this);
					}
				}

				switch(chn.rowCommand.volcmd)
				{
				case VOLCMD_VOLUME:
					memory.chnSettings[nChn].vol = chn.rowCommand.vol;
					break;
				case VOLCMD_VOLSLIDEUP:
				case VOLCMD_VOLSLIDEDOWN:
					if(chn.rowCommand.vol != 0)
						chn.nOldVolParam = chn.rowCommand.vol;
					break;
				case VOLCMD_TONEPORTAMENTO:
					if(chn.rowCommand.vol)
					{
						const auto [porta, clearEffectCommand] = GetVolCmdTonePorta(chn.rowCommand, 0);
						chn.portamentoSlide = porta;
						if(clearEffectCommand)
							command = CMD_NONE;
					}
					break;
				default:
					break;
				}
			}

			switch(command)
			{
			// Position Jump
			case CMD_POSITIONJUMP:
				PositionJump(playState, nChn);
				break;

			// Pattern Break
			case CMD_PATTERNBREAK:
				if(ROWINDEX row = PatternBreak(playState, nChn, param); row != ROWINDEX_INVALID)
					playState.m_breakRow = row;
			break;

			// Set Tempo
			case CMD_TEMPO:
				if(!m_playBehaviour[kMODVBlankTiming])
				{
					TEMPO tempo(CalculateXParam(playState.m_nPattern, playState.m_nRow, nChn), 0);
					if(GetType() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT))
					{
						if (tempo.GetInt()) chn.nOldTempo = static_cast<uint8>(tempo.GetInt()); else tempo.Set(chn.nOldTempo);
					}

					if(tempo >= GetMinimumTempoParam(GetType()))
					{
						playState.m_flags.set(SONG_FIRSTTICK, !m_playBehaviour[kMODTempoOnSecondTick]);
						SetTempo(playState, tempo, false);
					} else
					{
						// Tempo Slide
						playState.m_flags.reset(SONG_FIRSTTICK);
						for(uint32 i = 0; i < nonRowTicks; i++)
						{
							SetTempo(playState, tempo, false);
						}
					}
				}
				break;

			case CMD_S3MCMDEX:
				switch(param & 0xF0)
				{
				case 0xB0:  // Pattern Loop
					PatternLoop(playState, nChn, param & 0x0F);
					break;
				}
				break;

			case CMD_MODCMDEX:
				switch(param & 0xF0)
				{
				case 0x60:  // Pattern Loop
					PatternLoop(playState, nChn, param & 0x0F);
					break;
				}
				break;

			default:
				break;
			}

			// The following calculations are not interesting if we just want to get the song length...
			// ...unless we're playing a Face The Music module with scripts that may modify the speed or tempo based on some volume or pitch variable (see schlendering.ftm)
			if(!(adjustMode & eAdjust) && m_globalScript.empty())
				continue;

			ResetAutoSlides(chn);

			switch(command)
			{
			// Portamento Up/Down
			case CMD_PORTAMENTOUP:
				if(param)
				{
					// FT2 compatibility: Separate effect memory for all portamento commands
					// Test case: Porta-LinkMem.xm
					if(!m_playBehaviour[kFT2PortaUpDownMemory])
						chn.nOldPortaDown = param;
					chn.nOldPortaUp = param;
				}
				break;
			case CMD_PORTAMENTODOWN:
				if(param)
				{
					// FT2 compatibility: Separate effect memory for all portamento commands
					// Test case: Porta-LinkMem.xm
					if(!m_playBehaviour[kFT2PortaUpDownMemory])
						chn.nOldPortaUp = param;
					chn.nOldPortaDown = param;
				}
				break;
			// Tone-Portamento
			case CMD_TONEPORTAMENTO:
				if (param) chn.portamentoSlide = param;
				break;
			// Offset
			case CMD_OFFSET:
				if(param)
					chn.oldOffset = param << 8;
				break;
			// Volume Slide
			case CMD_VOLUMESLIDE:
			case CMD_TONEPORTAVOL:
				if (param) chn.nOldVolumeSlide = param;
				break;
			case CMD_AUTO_VOLUMESLIDE:
				AutoVolumeSlide(chn, param);
				break;
			case CMD_VOLUMEDOWN_ETX:
				VolumeDownETX(playState, chn, param);
				break;
			// Set Volume
			case CMD_VOLUME:
				memory.chnSettings[nChn].vol = param;
				break;
			case CMD_VOLUME8:
				memory.chnSettings[nChn].vol = static_cast<uint8>((param + 3u) / 4u);
				break;
			// Global Volume
			case CMD_GLOBALVOLUME:
				if(!(GetType() & GLOBALVOL_7BIT_FORMATS) && param < 128) param *= 2;
				// IT compatibility 16. ST3 and IT ignore out-of-range values
				if(param <= 128)
				{
					playState.m_nGlobalVolume = param * 2;
				} else if(!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_S3M)))
				{
					playState.m_nGlobalVolume = 256;
				}
				playState.Chn[m_playBehaviour[kPerChannelGlobalVolSlide] ? nChn : 0].autoSlide.SetActive(AutoSlideCommand::GlobalVolumeSlide, false);
				break;
			// Global Volume Slide
			case CMD_GLOBALVOLSLIDE:
				memory.GlobalVolSlide(playState.Chn[m_playBehaviour[kPerChannelGlobalVolSlide] ? nChn : 0], param, nonRowTicks);
				break;
			case CMD_CHANNELVOLUME:
				if (param <= 64) chn.nGlobalVol = static_cast<uint8>(param);
				break;
			case CMD_CHANNELVOLSLIDE:
				{
					if (param) chn.nOldChnVolSlide = param; else param = chn.nOldChnVolSlide;
					int32 volume = chn.nGlobalVol;
					if((param & 0x0F) == 0x0F && (param & 0xF0))
						volume += (param >> 4);		// Fine Up
					else if((param & 0xF0) == 0xF0 && (param & 0x0F))
						volume -= (param & 0x0F);	// Fine Down
					else if(param & 0x0F)			// Down
						volume -= (param & 0x0F) * nonRowTicks;
					else							// Up
						volume += ((param & 0xF0) >> 4) * nonRowTicks;
					Limit(volume, 0, 64);
					chn.nGlobalVol = static_cast<uint8>(volume);
				}
				break;
			case CMD_VOLUMEDOWN_DURATION:
				ChannelVolumeDownWithDuration(chn, param);
				break;
			case CMD_PANNING8:
				Panning(chn, param, Pan8bit);
				break;
			case CMD_MODCMDEX:
				switch(param & 0xF0)
				{
				case 0x00:  // LED filter
					for(CHANNELINDEX channel = 0; channel < GetNumChannels(); channel++)
					{
						playState.Chn[channel].dwFlags.set(CHN_AMIGAFILTER, !(param & 1));
					}
					break;

				case 0x80:  // Panning
					Panning(chn, (param & 0x0F), Pan4bit);
					break;

				case 0xF0:  // Active macro
					chn.nActiveMacro = param & 0x0F;
					break;
				}
				break;

			case CMD_S3MCMDEX:
				switch(param & 0xF0)
				{
				case 0x80:  // Panning
					Panning(chn, (param & 0x0F), Pan4bit);
					break;

				case 0x90:  // Extended channel effects
					// Change play direction is handled in adjustSamplePos case
					if (param < 0x9E)
						ExtendedChannelEffect(chn, param, playState);
					break;

				case 0xA0:  // High sample offset
					chn.nOldHiOffset = static_cast<uint8>(param);
					break;

				case 0xF0:  // Active macro
					chn.nActiveMacro = param & 0x0F;
					break;
				}
				break;

			case CMD_XFINEPORTAUPDOWN:
				// ignore high offset in compatible mode
				if (((param & 0xF0) == 0xA0) && !m_playBehaviour[kFT2RestrictXCommand])
					chn.nOldHiOffset = param & 0x0F;
				break;

			case CMD_VIBRATOVOL:
				if (param) chn.nOldVolumeSlide = param;
				param = 0;
				[[fallthrough]];
			case CMD_VIBRATO:
				Vibrato(chn, param);
				break;
			case CMD_FINEVIBRATO:
				FineVibrato(chn, param);
				break;
			case CMD_TREMOLO:
				Tremolo(chn, param);
				break;
			case CMD_PANBRELLO:
				Panbrello(chn, param);
				// Panbrello effect is permanent in compatible mode, so actually apply panbrello for the last tick of this row
				chn.nPanbrelloPos += static_cast<uint8>(chn.nPanbrelloSpeed * nonRowTicks);
				ProcessPanbrello(chn);
				break;

			case CMD_MIDI:
			case CMD_SMOOTHMIDI:
				if(param < 0x80)
					ProcessMIDIMacro(playState, nChn, false, m_MidiCfg.SFx[chn.nActiveMacro], chn.rowCommand.param, 0);
				else
					ProcessMIDIMacro(playState, nChn, false, m_MidiCfg.Zxx[param & 0x7F], chn.rowCommand.param, 0);
				break;

			default:
				break;
			}

			switch(chn.rowCommand.volcmd)
			{
			case VOLCMD_PANNING:
				Panning(chn, chn.rowCommand.vol, Pan6bit);
				break;

			case VOLCMD_VIBRATOSPEED:
				// FT2 does not automatically enable vibrato with the "set vibrato speed" command
				if(m_playBehaviour[kFT2VolColVibrato])
					chn.nVibratoSpeed = chn.rowCommand.vol & 0x0F;
				else
					Vibrato(chn, chn.rowCommand.vol << 4);
				break;
			case VOLCMD_VIBRATODEPTH:
				Vibrato(chn, chn.rowCommand.vol);
				break;

			default:
				break;
			}

			chn.isFirstTick = true;
			if(chn.autoSlide.IsActive(AutoSlideCommand::FineVolumeSlideUp) && command != CMD_AUTO_VOLUMESLIDE)
				FineVolumeUp(chn, 0, false);
			if(chn.autoSlide.IsActive(AutoSlideCommand::FineVolumeSlideDown) && command != CMD_AUTO_VOLUMESLIDE)
				FineVolumeDown(chn, 0, false);
			if(chn.autoSlide.IsActive(AutoSlideCommand::VolumeSlideSTK))
			{
				for(uint32 i = 0; i < numTicks; i++)
				{
					chn.isFirstTick = (i == 0);
					VolumeSlide(chn, 0);
				}
			}
			if(chn.autoSlide.IsActive(AutoSlideCommand::VolumeDownWithDuration))
			{
				chn.volSlideDownRemain -= std::min(chn.volSlideDownRemain, mpt::saturate_cast<uint16>(numTicks - 1));
				ChannelVolumeDownWithDuration(chn);
			}
			if(chn.autoSlide.IsActive(AutoSlideCommand::GlobalVolumeSlide) && command != CMD_GLOBALVOLSLIDE)
				memory.GlobalVolSlide(chn, chn.nOldGlobalVolSlide, nonRowTicks);
			if(command == CMD_VIBRATO || command == CMD_FINEVIBRATO || command == CMD_VIBRATOVOL || chn.autoSlide.IsActive(AutoSlideCommand::Vibrato))
			{
				uint32 vibTicks = ((GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) && !m_SongFlags[SONG_ITOLDEFFECTS]) ? numTicks : nonRowTicks;
				uint32 inc = chn.nVibratoSpeed * vibTicks;
				if(m_playBehaviour[kITVibratoTremoloPanbrello])
					inc *= 4;
				chn.nVibratoPos += static_cast<uint8>(inc);
			}
			if(command == CMD_TREMOLO || chn.autoSlide.IsActive(AutoSlideCommand::Tremolo))
			{
				uint32 tremTicks = ((GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) && !m_SongFlags[SONG_ITOLDEFFECTS]) ? numTicks : nonRowTicks;
				uint32 inc = chn.nTremoloSpeed * tremTicks;
				if(m_playBehaviour[kITVibratoTremoloPanbrello])
					inc *= 4;
				chn.nTremoloPos += static_cast<uint8>(inc);
			}
		
			if(m_playBehaviour[kST3EffectMemory] && command != CMD_NONE && param != 0)
			{
				UpdateS3MEffectMemory(chn, static_cast<ModCommand::PARAM>(param));
			}
		}

		if(chn.rowCommand.instr)
		{
			// Not necessarily consistent with actually playing instrument for IT compatibility
			chn.nOldIns = chn.rowCommand.instr;
		}

		ProcessAutoSlides(playState, nChn);
	} // for(...) end

	// Navigation Effects
	if(playState.m_flags[SONG_FIRSTTICK])
	{
		if(HandleNextRow(playState, Order(), true))
			playState.m_flags.set(SONG_BREAKTOROW);
	}
	return true;
}


bool CSoundFile::HandleNextRow(PlayState &state, const ModSequence &order, bool honorPatternLoop) const
{
	const bool doPatternLoop = (state.m_patLoopRow != ROWINDEX_INVALID);
	const bool doBreakRow = (state.m_breakRow != ROWINDEX_INVALID);
	const bool doPosJump = (state.m_posJump != ORDERINDEX_INVALID);
	bool breakToRow = false;

	// Pattern Break / Position Jump only if no loop running
	// Exception: FastTracker 2 in all cases, Impulse Tracker in case of position jump
	// Test case for FT2 exception: PatLoop-Jumps.xm, PatLoop-Various.xm
	// Test case for IT: exception: LoopBreak.it, sbx-priority.it
	if((doBreakRow || doPosJump)
	   && (!doPatternLoop
	       || m_playBehaviour[kFT2PatternLoopWithJumps]
	       || (m_playBehaviour[kITPatternLoopWithJumps] && doPosJump)
	       || (m_playBehaviour[kITPatternLoopWithJumpsOld] && doPosJump)))
	{
		if(!doPosJump)
			state.m_posJump = state.m_nCurrentOrder + 1;
		if(!doBreakRow)
			state.m_breakRow = 0;
		breakToRow = true;

		if(state.m_posJump >= order.size())
			state.m_posJump = order.GetRestartPos();

		// IT / FT2 compatibility: don't reset loop count on pattern break.
		// Test case: gm-trippy01.it, PatLoop-Break.xm, PatLoop-Weird.xm, PatLoop-Break.mod
		if(state.m_posJump != state.m_nCurrentOrder
		   && !m_playBehaviour[kITPatternLoopBreak] && !m_playBehaviour[kFT2PatternLoopWithJumps] && GetType() != MOD_TYPE_MOD)
		{
			for(CHANNELINDEX i = 0; i < GetNumChannels(); i++)
			{
				state.Chn[i].nPatternLoopCount = 0;
			}
		}

		state.m_nNextRow = state.m_breakRow;
		if(!honorPatternLoop || !m_PlayState.m_flags[SONG_PATTERNLOOP])
			state.m_nNextOrder = state.m_posJump;
	} else if(doPatternLoop)
	{
		// Pattern Loop
		state.m_nNextOrder = state.m_nCurrentOrder;
		state.m_nNextRow = state.m_patLoopRow;
		// FT2 skips the first row of the pattern loop if there's a pattern delay, ProTracker sometimes does it too (didn't quite figure it out yet).
		// But IT and ST3 don't do this.
		// Test cases: PatLoopWithDelay.it, PatLoopWithDelay.s3m
		if(state.m_nPatternDelay
		   && (GetType() != MOD_TYPE_IT || !m_playBehaviour[kITPatternLoopWithJumps])
		   && GetType() != MOD_TYPE_S3M)
		{
			state.m_nNextRow++;
		}

		// IT Compatibility: If the restart row is past the end of the current pattern
		// (e.g. when continued from a previous pattern without explicit SB0 effect), continue the next pattern.
		// Test case: LoopStartAfterPatternEnd.it
		if(state.m_patLoopRow >= Patterns[state.m_nPattern].GetNumRows())
		{
			state.m_nNextOrder++;
			state.m_nNextRow = 0;
		}
	}

	return breakToRow;
}


////////////////////////////////////////////////////////////
// Channels effects


void CSoundFile::ResetAutoSlides(ModChannel &chn) const
{
	const auto cmd = chn.rowCommand.command;
	const auto volcmd = chn.rowCommand.volcmd;
	if(cmd != CMD_NONE && GetType() == MOD_TYPE_669)
	{
		chn.autoSlide.Reset();
		return;
	}

	if((cmd == CMD_NONE || !chn.rowCommand.param) && chn.autoSlide.IsActive(AutoSlideCommand::VolumeSlideSTK))
		chn.autoSlide.SetActive(AutoSlideCommand::VolumeSlideSTK, false);
	if((cmd == CMD_CHANNELVOLUME || cmd == CMD_CHANNELVOLSLIDE) && chn.autoSlide.IsActive(AutoSlideCommand::VolumeDownWithDuration))
		chn.autoSlide.SetActive(AutoSlideCommand::VolumeDownWithDuration, false);

	if(chn.autoSlide.IsActive(AutoSlideCommand::FinePortamentoDown) || chn.autoSlide.IsActive(AutoSlideCommand::PortamentoDown)
	   || chn.autoSlide.IsActive(AutoSlideCommand::FinePortamentoUp) || chn.autoSlide.IsActive(AutoSlideCommand::PortamentoUp))
	{
		if(!chn.rowCommand.IsTonePortamento() && chn.rowCommand.IsAnyPitchSlide())
		{
			chn.autoSlide.SetActive(AutoSlideCommand::FinePortamentoDown, false);
			chn.autoSlide.SetActive(AutoSlideCommand::PortamentoDown, false);
			chn.autoSlide.SetActive(AutoSlideCommand::FinePortamentoUp, false);
			chn.autoSlide.SetActive(AutoSlideCommand::PortamentoUp, false);
		}
	}
	if(chn.autoSlide.IsActive(AutoSlideCommand::FineVolumeSlideUp) || chn.autoSlide.IsActive(AutoSlideCommand::FineVolumeSlideDown) || chn.autoSlide.IsActive(AutoSlideCommand::VolumeDownETX))
	{
		if(cmd == CMD_VOLUME || cmd == CMD_AUTO_VOLUMESLIDE || cmd == CMD_VOLUMEDOWN_ETX || chn.rowCommand.IsNormalVolumeSlide()
		   || volcmd == VOLCMD_VOLUME || volcmd == VOLCMD_VOLSLIDEUP || volcmd == VOLCMD_VOLSLIDEDOWN || volcmd == VOLCMD_FINEVOLUP || volcmd == VOLCMD_FINEVOLDOWN)
		{
			chn.autoSlide.SetActive(AutoSlideCommand::FineVolumeSlideUp, false);
			chn.autoSlide.SetActive(AutoSlideCommand::FineVolumeSlideDown, false);
			chn.autoSlide.SetActive(AutoSlideCommand::VolumeDownETX, false);
		}
	}
}


void CSoundFile::ProcessAutoSlides(PlayState &playState, CHANNELINDEX channel)
{
	ModChannel &chn = playState.Chn[channel];
	if(chn.autoSlide.IsActive(AutoSlideCommand::TonePortamento) && !chn.rowCommand.IsTonePortamento())
		TonePortamento(channel, chn.portamentoSlide);
	else if(chn.autoSlide.IsActive(AutoSlideCommand::TonePortamentoWithDuration))
		TonePortamentoWithDuration(chn);
	if(chn.autoSlide.IsActive(AutoSlideCommand::PortamentoUp))
		PortamentoUp(channel, chn.nOldPortaUp, true);
	else if(chn.autoSlide.IsActive(AutoSlideCommand::PortamentoDown))
		PortamentoDown(channel, chn.nOldPortaDown, true);
	else if(chn.autoSlide.IsActive(AutoSlideCommand::FinePortamentoUp))
		FinePortamentoUp(chn, chn.nOldFinePortaUpDown);
	else if(chn.autoSlide.IsActive(AutoSlideCommand::FinePortamentoDown))
		FinePortamentoDown(chn, chn.nOldFinePortaUpDown);
	if(chn.autoSlide.IsActive(AutoSlideCommand::PortamentoFC))
		PortamentoFC(chn);
	if(chn.autoSlide.IsActive(AutoSlideCommand::FineVolumeSlideUp) && chn.rowCommand.command != CMD_AUTO_VOLUMESLIDE)
		FineVolumeUp(chn, 0, false);
	if(chn.autoSlide.IsActive(AutoSlideCommand::FineVolumeSlideDown) && chn.rowCommand.command != CMD_AUTO_VOLUMESLIDE)
		FineVolumeDown(chn, 0, false);
	if(chn.autoSlide.IsActive(AutoSlideCommand::VolumeDownETX))
		chn.nVolume = std::max(int32(0), chn.nVolume - chn.nOldVolumeSlide);
	if(chn.autoSlide.IsActive(AutoSlideCommand::VolumeSlideSTK))
		VolumeSlide(chn, 0);
	if(chn.autoSlide.IsActive(AutoSlideCommand::GlobalVolumeSlide) && chn.rowCommand.command != CMD_GLOBALVOLSLIDE)
		GlobalVolSlide(playState, chn.nOldGlobalVolSlide, channel);
	if(chn.autoSlide.IsActive(AutoSlideCommand::VolumeDownWithDuration))
		ChannelVolumeDownWithDuration(chn);
	if(chn.autoSlide.IsActive(AutoSlideCommand::Vibrato))
		chn.dwFlags.set(CHN_VIBRATO);
	if(chn.autoSlide.IsActive(AutoSlideCommand::Tremolo))
		chn.dwFlags.set(CHN_TREMOLO);
}


// Update the effect memory of all S3M effects that use the last non-zero effect parameter as memory (Dxy, Exx, Fxx, Ixy, Jxy, Kxy, Lxy, Qxy, Rxy, Sxy)
// Test case: ParamMemory.s3m
void CSoundFile::UpdateS3MEffectMemory(ModChannel &chn, ModCommand::PARAM param) const
{
	chn.nOldVolumeSlide = param; // Dxy / Kxy / Lxy
	chn.nOldPortaUp = param;     // Exx / Fxx
	chn.nOldPortaDown = param;   // Exx / Fxx
	chn.nTremorParam = param;    // Ixy
	chn.nArpeggio = param;       // Jxy
	chn.nRetrigParam = param;    // Qxy
	chn.nTremoloDepth = (param & 0x0F) << 2;  // Rxy
	chn.nTremoloSpeed = (param >> 4) & 0x0F;  // Rxy
	chn.nOldCmdEx = param;                    // Sxy
}


// Calculate full parameter for effects that support parameter extension at the given pattern location.
// maxCommands sets the maximum number of XParam commands to look at for this effect
// extendedRows returns how many extended rows are used (i.e. a value of 0 means the command is not extended).
uint32 CSoundFile::CalculateXParam(PATTERNINDEX pat, ROWINDEX row, CHANNELINDEX chn, uint32 *extendedRows) const
{
	if(extendedRows != nullptr)
		*extendedRows = 0;
	if(!Patterns.IsValidPat(pat))
	{
#ifdef MPT_BUILD_FUZZER
		// Ending up in this situation implies a logic error
		std::abort();
#else
		return 0;
#endif
	}
	ROWINDEX maxCommands = 4;
	const ModCommand *m = Patterns[pat].GetpModCommand(row, chn);
	const auto startCmd = m->command;
	uint32 val = m->param;

	switch(m->command)
	{
	case CMD_OFFSET:
		// 24 bit command
		maxCommands = 2;
		break;
	case CMD_TEMPO:
	case CMD_PATTERNBREAK:
	case CMD_POSITIONJUMP:
	case CMD_FINETUNE:
	case CMD_FINETUNE_SMOOTH:
		// 16 bit command
		maxCommands = 1;
		break;
	default:
		return val;
	}

	const bool xmTempoFix = m->command == CMD_TEMPO && GetType() == MOD_TYPE_XM;
	ROWINDEX numRows = std::min(Patterns[pat].GetNumRows() - row - 1, maxCommands);
	uint32 extRows = 0;
	while(numRows > 0)
	{
		m += Patterns[pat].GetNumChannels();
		if(m->command != CMD_XPARAM)
			break;
		
		if(xmTempoFix && val >= 0x20 && val < 256)
		{
			// With XM, 0x20 is the lowest tempo. Anything below changes ticks per row.
			val -= 0x20;
		}
		val = (val << 8) | m->param;
		numRows--;
		extRows++;
	}

	// Always return a full-precision value for finetune
	if((startCmd == CMD_FINETUNE || startCmd == CMD_FINETUNE_SMOOTH) && !extRows)
		val <<= 8;
		
	if(extendedRows != nullptr)
		*extendedRows = extRows;

	return val;
}


void CSoundFile::PositionJump(PlayState &state, CHANNELINDEX chn) const
{
	state.m_nextPatStartRow = 0;  // FT2 E60 bug
	state.m_posJump = static_cast<ORDERINDEX>(CalculateXParam(state.m_nPattern, state.m_nRow, chn));

	// see https://forum.openmpt.org/index.php?topic=2769.0 - FastTracker resets Dxx if Bxx is called _after_ Dxx
	// Test case: PatternJump.mod
	if((GetType() & (MOD_TYPE_MOD | MOD_TYPE_XM)) && state.m_breakRow != ROWINDEX_INVALID)
	{
		state.m_breakRow = 0;
	}
}


ROWINDEX CSoundFile::PatternBreak(PlayState &state, CHANNELINDEX chn, uint8 param) const
{
	if(param >= 64 && (GetType() & MOD_TYPE_S3M))
	{
		// ST3 ignores invalid pattern breaks.
		return ROWINDEX_INVALID;
	}

	state.m_nextPatStartRow = 0; // FT2 E60 bug

	return static_cast<ROWINDEX>(CalculateXParam(state.m_nPattern, state.m_nRow, chn));
}


void CSoundFile::PortamentoFC(ModChannel &chn) const
{
	chn.fcPortaTick = !chn.fcPortaTick;
	if(!chn.fcPortaTick)
		return;
	chn.nPeriod -= static_cast<int8>(chn.nOldPortaUp) * 4;
}


void CSoundFile::PortamentoUp(CHANNELINDEX nChn, ModCommand::PARAM param, const bool doFinePortamentoAsRegular)
{
	PortamentoUp(m_PlayState, nChn, param, doFinePortamentoAsRegular);
	MidiPortamento(nChn, m_PlayState.Chn[nChn].nOldPortaUp, !doFinePortamentoAsRegular && UseCombinedPortamentoCommands());
}


void CSoundFile::PortamentoUp(PlayState &playState, CHANNELINDEX nChn, ModCommand::PARAM param, const bool doFinePortamentoAsRegular) const
{
	ModChannel &chn = playState.Chn[nChn];

	// IT compatibility: Initialize effect memory in the right order in case there are portamentos in both effect columns.
	// Test cases: DoubleSlide.it, DoubleSlideCompatGxx.it
	if(param && !m_playBehaviour[kITDoublePortamentoSlides])
	{
		// FT2 compatibility: Separate effect memory for all portamento commands
		// Test case: Porta-LinkMem.xm
		if(!m_playBehaviour[kFT2PortaUpDownMemory])
			chn.nOldPortaDown = param;
		chn.nOldPortaUp = param;
	} else
	{
		param = chn.nOldPortaUp;
	}

	const bool doFineSlides = !doFinePortamentoAsRegular && UseCombinedPortamentoCommands();

	if(GetType() == MOD_TYPE_MPT && chn.pModInstrument && chn.pModInstrument->pTuning)
	{
		// Portamento for instruments with custom tuning
		if(param >= 0xF0 && !doFinePortamentoAsRegular)
			PortamentoFineMPT(playState, nChn, param - 0xF0);
		else if(param >= 0xE0 && !doFinePortamentoAsRegular)
			PortamentoExtraFineMPT(chn, param - 0xE0);
		else
			PortamentoMPT(chn, param);
		return;
	} else if(GetType() == MOD_TYPE_PLM)
	{
		// A normal portamento up or down makes a follow-up tone portamento go the same direction.
		chn.nPortamentoDest = 1;
	}

	if (doFineSlides && param >= 0xE0)
	{
		if (param & 0x0F)
		{
			if ((param & 0xF0) == 0xF0)
			{
				FinePortamentoUp(chn, param & 0x0F);
				return;
			} else if ((param & 0xF0) == 0xE0 && GetType() != MOD_TYPE_DBM)
			{
				ExtraFinePortamentoUp(chn, param & 0x0F);
				return;
			}
		}
		if(GetType() != MOD_TYPE_DBM)
		{
			// DBM only has fine slides, no extra-fine slides.
			return;
		}
	}
	// Regular Slide
	if(!chn.isFirstTick
	   || (m_PlayState.m_nMusicSpeed == 1 && m_playBehaviour[kSlidesAtSpeed1])
	   || m_SongFlags[SONG_FASTPORTAS])
	{
		DoFreqSlide(chn, chn.nPeriod, param * 4);
	}
}


void CSoundFile::PortamentoDown(CHANNELINDEX nChn, ModCommand::PARAM param, const bool doFinePortamentoAsRegular)
{
	PortamentoDown(m_PlayState, nChn, param, doFinePortamentoAsRegular);
	MidiPortamento(nChn, -static_cast<int>(m_PlayState.Chn[nChn].nOldPortaDown), !doFinePortamentoAsRegular && UseCombinedPortamentoCommands());
}


void CSoundFile::PortamentoDown(PlayState &playState, CHANNELINDEX nChn, ModCommand::PARAM param, const bool doFinePortamentoAsRegular) const
{
	ModChannel &chn = playState.Chn[nChn];

	// IT compatibility: Initialize effect memory in the right order in case there are portamentos in both effect columns.
	// Test cases: DoubleSlide.it, DoubleSlideCompatGxx.it
	if(param && !m_playBehaviour[kITDoublePortamentoSlides])
	{
		// FT2 compatibility: Separate effect memory for all portamento commands
		// Test case: Porta-LinkMem.xm
		if(!m_playBehaviour[kFT2PortaUpDownMemory])
			chn.nOldPortaUp = param;
		chn.nOldPortaDown = param;
	} else
	{
		param = chn.nOldPortaDown;
	}

	const bool doFineSlides = !doFinePortamentoAsRegular && UseCombinedPortamentoCommands();

	if(GetType() == MOD_TYPE_MPT && chn.pModInstrument && chn.pModInstrument->pTuning)
	{
		// Portamento for instruments with custom tuning
		if(param >= 0xF0 && !doFinePortamentoAsRegular)
			PortamentoFineMPT(playState, nChn, -static_cast<int>(param - 0xF0));
		else if(param >= 0xE0 && !doFinePortamentoAsRegular)
			PortamentoExtraFineMPT(chn, -static_cast<int>(param - 0xE0));
		else
			PortamentoMPT(chn, -static_cast<int>(param));
		return;
	} else if(GetType() == MOD_TYPE_PLM)
	{
		// A normal portamento up or down makes a follow-up tone portamento go the same direction.
		chn.nPortamentoDest = 65535;
	}

	if(doFineSlides && param >= 0xE0)
	{
		if (param & 0x0F)
		{
			if ((param & 0xF0) == 0xF0)
			{
				FinePortamentoDown(chn, param & 0x0F);
				return;
			} else if ((param & 0xF0) == 0xE0 && GetType() != MOD_TYPE_DBM)
			{
				ExtraFinePortamentoDown(chn, param & 0x0F);
				return;
			}
		}
		if(GetType() != MOD_TYPE_DBM)
		{
			// DBM only has fine slides, no extra-fine slides.
			return;
		}
	}

	if(!chn.isFirstTick
	   || (m_PlayState.m_nMusicSpeed == 1 && m_playBehaviour[kSlidesAtSpeed1])
	   || m_SongFlags[SONG_FASTPORTAS])
	{
		DoFreqSlide(chn, chn.nPeriod, param * -4);
	}
}


// Send portamento commands to plugins
void CSoundFile::MidiPortamento(CHANNELINDEX nChn, int param, const bool doFineSlides)
{
	int actualParam = std::abs(param);
	int pitchBend = 0;

	// Old MIDI Pitch Bends:
	// - Applied on every tick
	// - No fine pitch slides (they are interpreted as normal slides)
	// New MIDI Pitch Bends:
	// - Behaviour identical to sample pitch bends if the instrument's PWD parameter corresponds to the actual VSTi setting.

	if(doFineSlides && actualParam >= 0xE0 && !m_playBehaviour[kOldMIDIPitchBends])
	{
		if(m_PlayState.Chn[nChn].isFirstTick)
		{
			// Extra fine slide...
			pitchBend = (actualParam & 0x0F) * mpt::signum(param);
			if(actualParam >= 0xF0)
			{
				// ... or just a fine slide!
				pitchBend *= 4;
			}
		}
	} else if(!m_PlayState.Chn[nChn].isFirstTick || m_playBehaviour[kOldMIDIPitchBends])
	{
		// Regular slide
		pitchBend = param * 4;
	}

	if(pitchBend)
	{
#ifndef NO_PLUGINS
		IMixPlugin *plugin = GetChannelInstrumentPlugin(m_PlayState.Chn[nChn]);
		if(plugin != nullptr)
		{
			int8 pwd = 13;	// Early OpenMPT legacy... Actually it's not *exactly* 13, but close enough...
			if(m_PlayState.Chn[nChn].pModInstrument != nullptr)
			{
				pwd = m_PlayState.Chn[nChn].pModInstrument->midiPWD;
			}
			plugin->MidiPitchBend(pitchBend, pwd, nChn);
		}
#endif // NO_PLUGINS
	}
}


void CSoundFile::FinePortamentoUp(ModChannel &chn, ModCommand::PARAM param) const
{
	MPT_ASSERT(!chn.HasCustomTuning());
	if(GetType() == MOD_TYPE_XM)
	{
		// FT2 compatibility: EAx / EBx memory is not linked
		// Test case: FineVol-LinkMem.xm
		if(param) chn.nOldFinePortaUpDown = (param << 4) | (chn.nOldFinePortaUpDown & 0x0F); else param = (chn.nOldFinePortaUpDown >> 4);
	} else if(GetType() == MOD_TYPE_MT2)
	{
		if(param) chn.nOldFinePortaUpDown = param; else param = chn.nOldFinePortaUpDown;
	}

	if(chn.isFirstTick && chn.nPeriod && param)
		DoFreqSlide(chn, chn.nPeriod, param * 4);
}


void CSoundFile::FinePortamentoDown(ModChannel &chn, ModCommand::PARAM param) const
{
	MPT_ASSERT(!chn.HasCustomTuning());
	if(GetType() == MOD_TYPE_XM)
	{
		// FT2 compatibility: EAx / EBx memory is not linked
		// Test case: FineVol-LinkMem.xm
		if(param) chn.nOldFinePortaUpDown = param | (chn.nOldFinePortaUpDown & 0xF0); else param = (chn.nOldFinePortaUpDown & 0x0F);
	} else if(GetType() == MOD_TYPE_MT2)
	{
		if(param) chn.nOldFinePortaUpDown = param; else param = chn.nOldFinePortaUpDown;
	}

	if(chn.isFirstTick && chn.nPeriod && param)
	{
		DoFreqSlide(chn, chn.nPeriod, param * -4);
		if(chn.nPeriod > 0xFFFF && !m_playBehaviour[kPeriodsAreHertz] && (!m_SongFlags[SONG_LINEARSLIDES] || GetType() == MOD_TYPE_XM))
			chn.nPeriod = 0xFFFF;
	}
}


void CSoundFile::ExtraFinePortamentoUp(ModChannel &chn, ModCommand::PARAM param) const
{
	MPT_ASSERT(!chn.HasCustomTuning());
	if(GetType() == MOD_TYPE_XM)
	{
		// FT2 compatibility: E1x / E2x / X1x / X2x memory is not linked
		// Test case: Porta-LinkMem.xm
		if(param) chn.nOldExtraFinePortaUpDown = (chn.nOldExtraFinePortaUpDown & 0x0F) | (param << 4); else param = (chn.nOldExtraFinePortaUpDown >> 4);
	} else if(GetType() == MOD_TYPE_MT2)
	{
		if(param) chn.nOldFinePortaUpDown = param; else param = chn.nOldFinePortaUpDown;
	}

	if(chn.isFirstTick && chn.nPeriod && param)
		DoFreqSlide(chn, chn.nPeriod, param);
}


void CSoundFile::ExtraFinePortamentoDown(ModChannel &chn, ModCommand::PARAM param) const
{
	MPT_ASSERT(!chn.HasCustomTuning());
	if(GetType() == MOD_TYPE_XM)
	{
		// FT2 compatibility: E1x / E2x / X1x / X2x memory is not linked
		// Test case: Porta-LinkMem.xm
		if(param) chn.nOldExtraFinePortaUpDown = (chn.nOldExtraFinePortaUpDown & 0xF0) | (param & 0x0F); else param = (chn.nOldExtraFinePortaUpDown & 0x0F);
	} else if(GetType() == MOD_TYPE_MT2)
	{
		if(param) chn.nOldFinePortaUpDown = param; else param = chn.nOldFinePortaUpDown;
	}

	if(chn.isFirstTick && chn.nPeriod && param)
	{
		DoFreqSlide(chn, chn.nPeriod, -static_cast<int32>(param));
		if(chn.nPeriod > 0xFFFF && !m_playBehaviour[kPeriodsAreHertz] && (!m_SongFlags[SONG_LINEARSLIDES] || GetType() == MOD_TYPE_XM))
			chn.nPeriod = 0xFFFF;
	}
}


// Process finetune command from pattern editor
void CSoundFile::ProcessFinetune(PATTERNINDEX pattern, ROWINDEX row, CHANNELINDEX channel, bool isSmooth)
{
	SetFinetune(pattern, row, channel, m_PlayState, isSmooth);
	// Also apply to notes played via CModDoc::PlayNote
	for(ModChannel &chn : m_PlayState.BackgroundChannels(*this))
	{
		if(chn.nMasterChn == channel + 1 && chn.isPreviewNote && !chn.dwFlags[CHN_KEYOFF])
			chn.microTuning = m_PlayState.Chn[channel].microTuning;
	}
}


void CSoundFile::SetFinetune(PATTERNINDEX pattern, ROWINDEX row, CHANNELINDEX channel, PlayState &playState, bool isSmooth) const
{
	ModChannel &chn = playState.Chn[channel];
	int16 newTuning = CalculateFinetuneTarget(pattern, row, channel);

	if(isSmooth)
	{
		const int32 ticksLeft = playState.TicksOnRow() - playState.m_nTickCount;
		if(ticksLeft > 1)
		{
			const int32 step = (newTuning - chn.microTuning) / ticksLeft;
			newTuning = mpt::saturate_cast<int16>(chn.microTuning + step);
		}
	}
	chn.microTuning = newTuning;

#ifndef NO_PLUGINS
	if(IMixPlugin *plugin = GetChannelInstrumentPlugin(chn); plugin != nullptr)
		plugin->MidiPitchBendRaw(chn.GetMIDIPitchBend(), channel);
#endif  // NO_PLUGINS
}


int16 CSoundFile::CalculateFinetuneTarget(PATTERNINDEX pattern, ROWINDEX row, CHANNELINDEX channel) const
{
	return mpt::saturate_cast<int16>(static_cast<int32>(CalculateXParam(pattern, row, channel, nullptr)) - 0x8000);
}


// Implemented for IMF / PTM / OKT compatibility, can't actually save this in any formats
// Slide up / down every x ticks by y semitones
// Oktalyzer: Slide down on first tick only, or on every tick
void CSoundFile::NoteSlide(ModChannel &chn, uint32 param, bool slideUp, bool retrig) const
{
	if(chn.isFirstTick)
	{
		if(param & 0xF0)
			chn.noteSlideParam = static_cast<uint8>(param & 0xF0) | (chn.noteSlideParam & 0x0F);
		if(param & 0x0F)
			chn.noteSlideParam = (chn.noteSlideParam & 0xF0) | static_cast<uint8>(param & 0x0F);
		chn.noteSlideCounter = (chn.noteSlideParam >> 4);
	}

	bool doTrigger = false;
	if(GetType() == MOD_TYPE_OKT)
		doTrigger = ((chn.noteSlideParam & 0xF0) == 0x10) || m_PlayState.m_flags[SONG_FIRSTTICK];
	else
		doTrigger = !chn.isFirstTick && (--chn.noteSlideCounter == 0);

	if(doTrigger)
	{
		const uint8 speed = (chn.noteSlideParam >> 4), steps = (chn.noteSlideParam & 0x0F);
		chn.noteSlideCounter = speed;
		// update it
		const int32 delta = (slideUp ? steps : -steps);
		if(chn.HasCustomTuning())
			chn.m_PortamentoFineSteps += delta * chn.pModInstrument->pTuning->GetFineStepCount();
		else
			chn.nPeriod = GetPeriodFromNote(delta + GetNoteFromPeriod(chn.nPeriod, chn.nFineTune, chn.nC5Speed), chn.nFineTune, chn.nC5Speed);

		if(retrig)
			chn.position.Set(0);
	}
}


std::pair<uint16, bool> CSoundFile::GetVolCmdTonePorta(const ModCommand &m, uint32 startTick) const
{
	if(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_AMS | MOD_TYPE_DMF | MOD_TYPE_DBM | MOD_TYPE_IMF | MOD_TYPE_PSM | MOD_TYPE_J2B | MOD_TYPE_ULT | MOD_TYPE_OKT | MOD_TYPE_MT2 | MOD_TYPE_MDL))
	{
		return {ImpulseTrackerPortaVolCmd[m.vol & 0x0F], false};
	} else
	{
		bool clearEffectColumn = false;
		uint16 vol = m.vol;
		if(m.command == CMD_TONEPORTAMENTO && GetType() == MOD_TYPE_XM)
		{
			// Yes, FT2 is *that* weird. If there is a Mx command in the volume column
			// and a normal 3xx command, the 3xx command is ignored but the Mx command's
			// effectiveness is doubled.
			// Test case: TonePortamentoMemory.xm
			clearEffectColumn = true;
			vol *= 2;
		}

		// FT2 compatibility: If there's a portamento and a note delay, execute the portamento, but don't update the parameter
		// Test case: PortaDelay.xm
		if(m_playBehaviour[kFT2PortaDelay] && startTick != 0)
			return {uint16(0), clearEffectColumn};
		else
			return {static_cast<uint16>(vol * 16), clearEffectColumn};
	}
}


bool CSoundFile::TonePortamentoSharesEffectMemory() const
{
	return (!m_SongFlags[SONG_ITCOMPATGXX] && m_playBehaviour[kITPortaMemoryShare]) || GetType() == MOD_TYPE_PLM;
}


void CSoundFile::InitTonePortamento(ModChannel &chn, uint16 param) const
{
	// IT compatibility 03: Share effect memory with portamento up/down
	if(TonePortamentoSharesEffectMemory())
	{
		if(param == 0)
			param = chn.nOldPortaUp;
		chn.nOldPortaUp = chn.nOldPortaDown = static_cast<uint8>(param);
	}

	if(param)
		chn.portamentoSlide = param;
}


void CSoundFile::TonePortamento(CHANNELINDEX nChn, uint16 param)
{
	auto delta = TonePortamento(m_PlayState, nChn, param);
	if(!delta)
		return;

#ifndef NO_PLUGINS
	ModChannel &chn = m_PlayState.Chn[nChn];
	if(!m_playBehaviour[kPluginIgnoreTonePortamento] && chn.pModInstrument != nullptr && chn.pModInstrument->midiPWD != 0)
	{
		IMixPlugin *plugin = GetChannelInstrumentPlugin(chn);
		if(plugin != nullptr)
		{
			plugin->MidiTonePortamento(delta, chn.GetPluginNote(true), chn.pModInstrument->midiPWD, nChn);
		}
	}
#endif  // NO_PLUGINS
}


// Portamento Slide
int32 CSoundFile::TonePortamento(PlayState &playState, CHANNELINDEX nChn, uint16 param) const
{
	ModChannel &chn = playState.Chn[nChn];
	chn.dwFlags.set(CHN_PORTAMENTO);
	if(m_SongFlags[SONG_AUTO_TONEPORTA])
		chn.autoSlide.SetActive(AutoSlideCommand::TonePortamento, param != 0 || m_SongFlags[SONG_AUTO_TONEPORTA_CONT]);

	// IT compatibility: Initialize effect memory in the right order in case there are portamentos in both effect columns.
	// Test cases: DoubleSlide.it, DoubleSlideCompatGxx.it
	if(!m_playBehaviour[kITDoublePortamentoSlides])
		InitTonePortamento(chn, param);
	int32 delta = chn.portamentoSlide;

	if(chn.HasCustomTuning())
	{
		//Behavior: Param tells number of finesteps(or 'fullsteps'(notes) with glissando)
		//to slide per row(not per tick).
		if(delta == 0)
			return 0;

		const int32 oldPortamentoTickSlide = (playState.m_nTickCount != 0) ? chn.m_PortamentoTickSlide : 0;

		if(chn.nPortamentoDest < 0)
			delta = -delta;

		chn.m_PortamentoTickSlide = static_cast<int32>((playState.m_nTickCount + 1.0) * delta / playState.m_nMusicSpeed);

		if(chn.dwFlags[CHN_GLISSANDO])
		{
			chn.m_PortamentoTickSlide *= chn.pModInstrument->pTuning->GetFineStepCount() + 1;
			//With glissando interpreting param as notes instead of finesteps.
		}

		const int32 slide = chn.m_PortamentoTickSlide - oldPortamentoTickSlide;

		if(std::abs(chn.nPortamentoDest) <= std::abs(slide))
		{
			if(chn.nPortamentoDest != 0)
			{
				chn.m_PortamentoFineSteps += chn.nPortamentoDest;
				chn.nPortamentoDest = 0;
				chn.m_CalculateFreq = true;
			}
		} else
		{
			chn.m_PortamentoFineSteps += slide;
			chn.nPortamentoDest -= slide;
			chn.m_CalculateFreq = true;
		}

		return 0;
	}

	// ST3: Adlib Note + Tone Portamento does not execute the slide, but changes to the target note instantly on the next row (unless there is another note with tone portamento)
	// Test case: TonePortamentoWithAdlibNote.s3m
	if(m_playBehaviour[kST3TonePortaWithAdlibNote] && chn.dwFlags[CHN_ADLIB] && chn.rowCommand.IsNote())
		return 0;

	bool doPorta = !chn.isFirstTick
	               || GetType() == MOD_TYPE_DBM
	               || (playState.m_nMusicSpeed == 1 && m_playBehaviour[kSlidesAtSpeed1])
	               || m_SongFlags[SONG_FASTPORTAS];

	if(GetType() == MOD_TYPE_PLM && delta >= 0xF0)
	{
		delta -= 0xF0;
		doPorta = chn.isFirstTick;
	}
	delta *= (GetType() == MOD_TYPE_669) ? 2 : 4;

	if(chn.nPeriod && chn.nPortamentoDest && doPorta)
	{
		const int32 actualDelta = PeriodsAreFrequencies() ? delta : -delta;
		// IT compatibility: Command Lxx, with no tone portamento set up before, will always execute the "portamento down" branch.
		// Test cases: LxxWith0Portamento-Linear.it, LxxWith0Portamento-Amiga.it
		if(m_playBehaviour[kITDoublePortamentoSlides] && !delta && chn.rowCommand.command == CMD_TONEPORTAVOL)
		{
			if(chn.nPeriod > 1 && m_SongFlags[SONG_LINEARSLIDES])
				chn.nPeriod--;
			if(chn.nPeriod < chn.nPortamentoDest)
				chn.nPeriod = chn.nPortamentoDest;
		} else if(chn.nPeriod < chn.nPortamentoDest || chn.portaTargetReached)
		{
			DoFreqSlide(chn, chn.nPeriod, actualDelta, true);
			if(chn.nPeriod > chn.nPortamentoDest)
				chn.nPeriod = chn.nPortamentoDest;
		} else if(chn.nPeriod > chn.nPortamentoDest)
		{
			DoFreqSlide(chn, chn.nPeriod, -actualDelta, true);
			if(chn.nPeriod < chn.nPortamentoDest)
				chn.nPeriod = chn.nPortamentoDest;
			// FT2 compatibility: Reaching portamento target from below forces subsequent portamentos on the same note to use the logic for reaching the note from above instead.
			// Test case: PortaResetDirection.xm
			if(chn.nPeriod == chn.nPortamentoDest && m_playBehaviour[kFT2PortaResetDirection])
				chn.portaTargetReached = true;
		}
	}

	// IT compatibility 23. Portamento with no note
	// ProTracker also disables portamento once the target is reached.
	// Test case: PortaTarget.mod
	if(chn.nPeriod == chn.nPortamentoDest && (m_playBehaviour[kITPortaTargetReached] || GetType() == MOD_TYPE_MOD))
		chn.nPortamentoDest = 0;

	return doPorta ? delta : 0;
}


void CSoundFile::TonePortamentoWithDuration(ModChannel &chn, uint16 param) const
{
	if(param != uint16_max)
	{
		// Prepare portamento
		if(!chn.rowCommand.IsNote())
			return;
		chn.autoSlide.SetActive(AutoSlideCommand::TonePortamentoWithDuration, param != 0);
		if(param == 0)
		{
			chn.nPeriod = chn.nPortamentoDest;
			return;
		}
		uint32 sourceNote = GetNoteFromPeriod(chn.nPeriod, chn.nFineTune, chn.nC5Speed);
		chn.portamentoSlide = static_cast<uint16>(Util::muldivr_unsigned(std::abs(static_cast<int>(chn.rowCommand.note - sourceNote)), 64, m_PlayState.m_nMusicSpeed * param));
	} else if(chn.nPeriod && chn.nPortamentoDest)
	{
		// Run portamento
		chn.dwFlags.set(CHN_PORTAMENTO);
		const int32 actualDelta = PeriodsAreFrequencies() ? chn.portamentoSlide : -chn.portamentoSlide;
		if(chn.nPeriod < chn.nPortamentoDest)
		{
			DoFreqSlide(chn, chn.nPeriod, actualDelta, true);
			if(chn.nPeriod >= chn.nPortamentoDest)
			{
				chn.nPeriod = chn.nPortamentoDest;
				chn.nPortamentoDest = 0;
			}
		} else if(chn.nPeriod > chn.nPortamentoDest)
		{
			DoFreqSlide(chn, chn.nPeriod, -actualDelta, true);
			if(chn.nPeriod <= chn.nPortamentoDest)
			{
				chn.nPeriod = chn.nPortamentoDest;
				chn.nPortamentoDest = 0;
			}
		}
	}
}


void CSoundFile::Vibrato(ModChannel &chn, uint32 param) const
{
	if (param & 0x0F) chn.nVibratoDepth = (param & 0x0F) * 4;
	if (param & 0xF0) chn.nVibratoSpeed = (param >> 4) & 0x0F;
	if(m_SongFlags[SONG_AUTO_VIBRATO])
		chn.autoSlide.SetActive(AutoSlideCommand::Vibrato, param != 0);
	else
		chn.dwFlags.set(CHN_VIBRATO);
}


void CSoundFile::FineVibrato(ModChannel &chn, uint32 param) const
{
	if (param & 0x0F) chn.nVibratoDepth = param & 0x0F;
	if (param & 0xF0) chn.nVibratoSpeed = (param >> 4) & 0x0F;
	if(m_SongFlags[SONG_AUTO_VIBRATO])
		chn.autoSlide.SetActive(AutoSlideCommand::Vibrato, param != 0);
	else
		chn.dwFlags.set(CHN_VIBRATO);
	// ST3 compatibility: Do not distinguish between vibrato types in effect memory
	// Test case: VibratoTypeChange.s3m
	if(m_playBehaviour[kST3VibratoMemory] && (param & 0x0F))
	{
		chn.nVibratoDepth *= 4u;
	}
}


void CSoundFile::Panbrello(ModChannel &chn, uint32 param) const
{
	if (param & 0x0F) chn.nPanbrelloDepth = param & 0x0F;
	if (param & 0xF0) chn.nPanbrelloSpeed = (param >> 4) & 0x0F;
}


void CSoundFile::Panning(ModChannel &chn, uint32 param, PanningType panBits) const
{
	// No panning in ProTracker mode
	if(m_playBehaviour[kMODIgnorePanning])
	{
		return;
	}
	// IT Compatibility (and other trackers as well): panning disables surround (unless panning in rear channels is enabled, which is not supported by the original trackers anyway)
	if (!m_PlayState.m_flags[SONG_SURROUNDPAN] && (panBits == Pan8bit || m_playBehaviour[kPanOverride]))
	{
		chn.dwFlags.reset(CHN_SURROUND);
	}
	if(panBits == Pan4bit)
	{
		// 0...15 panning
		chn.nPan = (param * 256 + 8) / 15;
	} else if(panBits == Pan6bit)
	{
		// 0...64 panning
		if(param > 64) param = 64;
		chn.nPan = param * 4;
	} else
	{
		if(!(GetType() & (MOD_TYPE_S3M | MOD_TYPE_DSM | MOD_TYPE_AMF0 | MOD_TYPE_AMF | MOD_TYPE_MTM)))
		{
			// Real 8-bit panning
			chn.nPan = param;
		} else
		{
			// 7-bit panning + surround
			if(param <= 0x80)
			{
				chn.nPan = param << 1;
			} else if(param == 0xA4)
			{
				chn.dwFlags.set(CHN_SURROUND);
				chn.nPan = 0x80;
			}
		}
	}

	chn.dwFlags.set(CHN_FASTVOLRAMP);
	chn.nRestorePanOnNewNote = 0;
	//IT compatibility 20. Set pan overrides random pan
	if(m_playBehaviour[kPanOverride])
	{
		chn.nPanSwing = 0;
		chn.nPanbrelloOffset = 0;
	}
}


void CSoundFile::AutoVolumeSlide(ModChannel &chn, ModCommand::PARAM param) const
{
	if(m_SongFlags[SONG_AUTO_VOLSLIDE_STK])
	{
		chn.nOldVolumeSlide = param;
		chn.autoSlide.SetActive(AutoSlideCommand::VolumeSlideSTK);
	} else
	{
		if(param & 0x0F)
		{
			FineVolumeDown(chn, param, false);
			chn.autoSlide.SetActive(AutoSlideCommand::FineVolumeSlideDown);
		} else
		{
			FineVolumeUp(chn, param, false);
			chn.autoSlide.SetActive(AutoSlideCommand::FineVolumeSlideUp);
		}
	}
}


void CSoundFile::VolumeDownETX(const PlayState &playState, ModChannel &chn, ModCommand::PARAM param) const
{
	chn.autoSlide.SetActive(AutoSlideCommand::VolumeDownETX, param != 0);
	if(!param || !playState.m_nSamplesPerTick)
		return;
	const uint32 slideDuration = Util::muldivr_unsigned(m_MixerSettings.gdwMixingFreq, 600, 1000) / param;  // 600ms at maximum volume
	const uint32 neededTicks = std::max(uint32(1), (slideDuration + playState.m_nSamplesPerTick / 2u) / playState.m_nSamplesPerTick);
	chn.nOldVolumeSlide = mpt::saturate_cast<uint8>(256 / neededTicks);
}


void CSoundFile::VolumeSlide(ModChannel &chn, ModCommand::PARAM param) const
{
	if (param)
		chn.nOldVolumeSlide = param;
	else
		param = chn.nOldVolumeSlide;

	if((GetType() & (MOD_TYPE_MOD | MOD_TYPE_XM | MOD_TYPE_MT2 | MOD_TYPE_MED | MOD_TYPE_DIGI | MOD_TYPE_STP | MOD_TYPE_DTM)))
	{
		// MOD / XM nibble priority
		if((param & 0xF0) != 0)
		{
			param &= 0xF0;
		} else
		{
			param &= 0x0F;
		}
	}

	int newVolume = chn.nVolume;
	if(!(GetType() & (MOD_TYPE_MOD | MOD_TYPE_XM | MOD_TYPE_AMF0 | MOD_TYPE_MED | MOD_TYPE_DIGI)))
	{
		if ((param & 0x0F) == 0x0F) //Fine upslide or slide -15
		{
			if (param & 0xF0) //Fine upslide
			{
				FineVolumeUp(chn, (param >> 4), false);
				return;
			} else //Slide -15
			{
				if(chn.isFirstTick && !m_SongFlags[SONG_FASTVOLSLIDES])
				{
					newVolume -= 0x0F * 4;
				}
			}
		} else
		if ((param & 0xF0) == 0xF0) //Fine downslide or slide +15
		{
			if (param & 0x0F) //Fine downslide
			{
				FineVolumeDown(chn, (param & 0x0F), false);
				return;
			} else //Slide +15
			{
				if(chn.isFirstTick && !m_SongFlags[SONG_FASTVOLSLIDES])
				{
					newVolume += 0x0F * 4;
				}
			}
		}
	}
	if(!chn.isFirstTick || m_SongFlags[SONG_FASTVOLSLIDES] || (m_PlayState.m_nMusicSpeed == 1 && GetType() == MOD_TYPE_DBM))
	{
		// IT compatibility: Ignore slide commands with both nibbles set.
		if (param & 0x0F)
		{
			if(!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) || (param & 0xF0) == 0)
				newVolume -= (int)((param & 0x0F) * 4);
		}
		else
		{
			newVolume += (int)((param & 0xF0) >> 2);
		}
		if (GetType() == MOD_TYPE_MOD) chn.dwFlags.set(CHN_FASTVOLRAMP);
	}
	newVolume = Clamp(newVolume, 0, 256);

	chn.nVolume = newVolume;
}


void CSoundFile::PanningSlide(ModChannel &chn, ModCommand::PARAM param, bool memory) const
{
	if(memory)
	{
		// FT2 compatibility: Use effect memory (lxx and rxx in XM shouldn't use effect memory).
		// Test case: PanSlideMem.xm
		if(param)
			chn.nOldPanSlide = param;
		else
			param = chn.nOldPanSlide;
	}

	if((GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2)))
	{
		// XM nibble priority
		if((param & 0xF0) != 0)
		{
			param &= 0xF0;
		} else
		{
			param &= 0x0F;
		}
	}

	int32 nPanSlide = 0;

	if(!(GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2)))
	{
		if (((param & 0x0F) == 0x0F) && (param & 0xF0))
		{
			if(m_PlayState.m_flags[SONG_FIRSTTICK])
			{
				param = (param & 0xF0) / 4u;
				nPanSlide = - (int)param;
			}
		} else if (((param & 0xF0) == 0xF0) && (param & 0x0F))
		{
			if(m_PlayState.m_flags[SONG_FIRSTTICK])
			{
				nPanSlide = (param & 0x0F) * 4u;
			}
		} else if(!m_PlayState.m_flags[SONG_FIRSTTICK])
		{
			if (param & 0x0F)
			{
				// IT compatibility: Ignore slide commands with both nibbles set.
				if(!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) || (param & 0xF0) == 0)
					nPanSlide = (int)((param & 0x0F) * 4u);
			} else
			{
				nPanSlide = -(int)((param & 0xF0) / 4u);
			}
		}
	} else
	{
		if(!m_PlayState.m_flags[SONG_FIRSTTICK])
		{
			if (param & 0xF0)
			{
				nPanSlide = (int)((param & 0xF0) / 4u);
			} else
			{
				nPanSlide = -(int)((param & 0x0F) * 4u);
			}
			// FT2 compatibility: FT2's panning slide is like IT's fine panning slide (not as deep)
			if(m_playBehaviour[kFT2PanSlide])
				nPanSlide /= 4;
		}
	}
	if (nPanSlide)
	{
		nPanSlide += chn.nPan;
		nPanSlide = Clamp(nPanSlide, 0, 256);
		chn.nPan = nPanSlide;
		chn.nRestorePanOnNewNote = 0;
	}
}


void CSoundFile::FineVolumeUp(ModChannel &chn, ModCommand::PARAM param, bool volCol) const
{
	if(GetType() == MOD_TYPE_XM)
	{
		// FT2 compatibility: EAx / EBx memory is not linked
		// Test case: FineVol-LinkMem.xm
		if(param) chn.nOldFineVolUpDown = (param << 4) | (chn.nOldFineVolUpDown & 0x0F); else param = (chn.nOldFineVolUpDown >> 4);
	} else if(volCol)
	{
		if(param) chn.nOldVolParam = param; else param = chn.nOldVolParam;
	} else
	{
		if(param) chn.nOldFineVolUpDown = param; else param = chn.nOldFineVolUpDown;
	}

	if(chn.isFirstTick)
	{
		chn.nVolume += param * 4;
		if(chn.nVolume > 256) chn.nVolume = 256;
		if(GetType() & MOD_TYPE_MOD) chn.dwFlags.set(CHN_FASTVOLRAMP);
	}
}


void CSoundFile::FineVolumeDown(ModChannel &chn, ModCommand::PARAM param, bool volCol) const
{
	if(GetType() == MOD_TYPE_XM)
	{
		// FT2 compatibility: EAx / EBx memory is not linked
		// Test case: FineVol-LinkMem.xm
		if(param) chn.nOldFineVolUpDown = param | (chn.nOldFineVolUpDown & 0xF0); else param = (chn.nOldFineVolUpDown & 0x0F);
	} else if(volCol)
	{
		if(param) chn.nOldVolParam = param; else param = chn.nOldVolParam;
	} else
	{
		if(param) chn.nOldFineVolUpDown = param; else param = chn.nOldFineVolUpDown;
	}

	if(chn.isFirstTick)
	{
		chn.nVolume -= param * 4;
		if(chn.nVolume < 0) chn.nVolume = 0;
		if(GetType() & MOD_TYPE_MOD) chn.dwFlags.set(CHN_FASTVOLRAMP);
	}
}


void CSoundFile::Tremolo(ModChannel &chn, uint32 param) const
{
	if (param & 0x0F) chn.nTremoloDepth = (param & 0x0F) << 2;
	if (param & 0xF0) chn.nTremoloSpeed = (param >> 4) & 0x0F;
	if(m_SongFlags[SONG_AUTO_TREMOLO])
		chn.autoSlide.SetActive(AutoSlideCommand::Tremolo, (param & 0x0F) != 0);
	else
		chn.dwFlags.set(CHN_TREMOLO);
}


void CSoundFile::ChannelVolSlide(ModChannel &chn, ModCommand::PARAM param) const
{
	int32 nChnSlide = 0;
	if (param) chn.nOldChnVolSlide = param; else param = chn.nOldChnVolSlide;

	if (((param & 0x0F) == 0x0F) && (param & 0xF0))
	{
		if(m_PlayState.m_flags[SONG_FIRSTTICK]) nChnSlide = param >> 4;
	} else if (((param & 0xF0) == 0xF0) && (param & 0x0F))
	{
		if(m_PlayState.m_flags[SONG_FIRSTTICK]) nChnSlide = - (int)(param & 0x0F);
	} else
	{
		if(!m_PlayState.m_flags[SONG_FIRSTTICK])
		{
			if (param & 0x0F)
			{
				if(!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_J2B | MOD_TYPE_DBM)) || (param & 0xF0) == 0)
					nChnSlide = -(int)(param & 0x0F);
			} else
			{
				nChnSlide = (int)((param & 0xF0) >> 4);
			}
		}
	}
	if (nChnSlide)
	{
		nChnSlide += chn.nGlobalVol;
		Limit(nChnSlide, 0, 64);
		chn.nGlobalVol = static_cast<uint8>(nChnSlide);
	}
}


void CSoundFile::ChannelVolumeDownWithDuration(ModChannel &chn, uint16 param) const
{
	if(param != uint16_max)
	{
		// Prepare slide
		chn.autoSlide.SetActive(AutoSlideCommand::VolumeDownWithDuration, param != 0);
		if(param == 0)
		{
			chn.nGlobalVol = 0;
			return;
		}
		chn.volSlideDownStart = chn.nGlobalVol;
		chn.volSlideDownTotal = chn.volSlideDownRemain = mpt::saturate_cast<uint16>(param * m_PlayState.m_nMusicSpeed);
	} else if(chn.volSlideDownTotal)
	{
		// Run slide
		if(chn.volSlideDownRemain)
			chn.nGlobalVol = static_cast<uint8>(Util::muldivr(chn.volSlideDownStart, --chn.volSlideDownRemain, chn.volSlideDownTotal));
		else
			chn.nGlobalVol = 0;
	}
}


void CSoundFile::ExtendedMODCommands(CHANNELINDEX nChn, ModCommand::PARAM param)
{
	ModChannel &chn = m_PlayState.Chn[nChn];
	uint8 command = param & 0xF0;
	param &= 0x0F;
	switch(command)
	{
	// E0x: Set Filter
	case 0x00:
		for(CHANNELINDEX channel = 0; channel < GetNumChannels(); channel++)
		{
			m_PlayState.Chn[channel].dwFlags.set(CHN_AMIGAFILTER, !(param & 1));
		}
		break;
	// E1x: Fine Portamento Up
	case 0x10:
		if(param || (GetType() & (MOD_TYPE_XM|MOD_TYPE_MT2)))
		{
			FinePortamentoUp(chn, param);
			if(!m_playBehaviour[kPluginIgnoreTonePortamento])
				MidiPortamento(nChn, 0xF0 | param, true);
		}
		break;
	// E2x: Fine Portamento Down
	case 0x20:
		if(param || (GetType() & (MOD_TYPE_XM|MOD_TYPE_MT2)))
		{
			FinePortamentoDown(chn, param);
			if(!m_playBehaviour[kPluginIgnoreTonePortamento])
				MidiPortamento(nChn, -static_cast<int>(0xF0 | param), true);
		}
		break;
	// E3x: Set Glissando Control
	case 0x30:	chn.dwFlags.set(CHN_GLISSANDO, param != 0); break;
	// E4x: Set Vibrato WaveForm
	case 0x40:	chn.nVibratoType = param & 0x07; break;
	// E5x: Set FineTune
	case 0x50:	if(!m_PlayState.m_flags[SONG_FIRSTTICK])
					break;
				if(GetType() & (MOD_TYPE_MOD | MOD_TYPE_DIGI | MOD_TYPE_AMF0 | MOD_TYPE_MED))
				{
					chn.nFineTune = MOD2XMFineTune(param);
					if(chn.nPeriod && chn.rowCommand.IsNote()) chn.nPeriod = GetPeriodFromNote(chn.nNote, chn.nFineTune, chn.nC5Speed);
				} else if(GetType() == MOD_TYPE_MTM)
				{
					if(chn.rowCommand.IsNote() && chn.pModSample != nullptr)
					{
						// Effect is permanent in MultiTracker
						const_cast<ModSample *>(chn.pModSample)->nFineTune = param;
						chn.nFineTune = param;
						if(chn.nPeriod) chn.nPeriod = GetPeriodFromNote(chn.nNote, chn.nFineTune, chn.nC5Speed);
					}
				} else if(chn.rowCommand.IsNote())
				{
					chn.nFineTune = MOD2XMFineTune(param - 8);
					if(chn.nPeriod) chn.nPeriod = GetPeriodFromNote(chn.nNote, chn.nFineTune, chn.nC5Speed);
				}
				break;
	// E6x: Pattern Loop
	case 0x60:
		if(m_PlayState.m_flags[SONG_FIRSTTICK])
			PatternLoop(m_PlayState, nChn, param & 0x0F);
		break;
	// E7x: Set Tremolo WaveForm
	case 0x70:	chn.nTremoloType = param & 0x07; break;
	// E8x: Set 4-bit Panning
	case 0x80:
		if(m_PlayState.m_flags[SONG_FIRSTTICK])
		{
			Panning(chn, param, Pan4bit);
		}
		break;
	// E9x: Retrig
	case 0x90:	RetrigNote(nChn, param); break;
	// EAx: Fine Volume Up
	case 0xA0:	if ((param) || (GetType() & (MOD_TYPE_XM|MOD_TYPE_MT2))) FineVolumeUp(chn, param, false); break;
	// EBx: Fine Volume Down
	case 0xB0:	if ((param) || (GetType() & (MOD_TYPE_XM|MOD_TYPE_MT2))) FineVolumeDown(chn, param, false); break;
	// ECx: Note Cut
	case 0xC0:	NoteCut(nChn, param, false); break;
	// EDx: Note Delay
	// EEx: Pattern Delay
	case 0xF0:
		if(GetType() == MOD_TYPE_MOD) // MOD: Invert Loop
		{
			chn.nEFxSpeed = param;
			if(m_PlayState.m_flags[SONG_FIRSTTICK]) InvertLoop(chn);
		} else // XM: Set Active Midi Macro
		{
			chn.nActiveMacro = param;
		}
		break;
	}
}


void CSoundFile::ExtendedS3MCommands(CHANNELINDEX nChn, ModCommand::PARAM param)
{
	ModChannel &chn = m_PlayState.Chn[nChn];
	uint8 command = param & 0xF0;
	param &= 0x0F;
	switch(command)
	{
	// S0x: Set Filter
	// S1x: Set Glissando Control
	case 0x10:	chn.dwFlags.set(CHN_GLISSANDO, param != 0); break;
	// S2x: Set FineTune
	case 0x20:	if(!m_PlayState.m_flags[SONG_FIRSTTICK])
					break;
				if(chn.HasCustomTuning())
				{
					chn.nFineTune = param - 8;
					chn.m_CalculateFreq = true;
				} else if(GetType() != MOD_TYPE_669)
				{
					chn.nC5Speed = S3MFineTuneTable[param];
					chn.nFineTune = MOD2XMFineTune(param);
					if(chn.nPeriod)
						chn.nPeriod = GetPeriodFromNote(chn.nNote, chn.nFineTune, chn.nC5Speed);
				} else if(chn.pModSample != nullptr)
				{
					chn.nC5Speed = chn.pModSample->nC5Speed + param * 80;
				}
				break;
	// S3x: Set Vibrato Waveform
	case 0x30:	if(GetType() == MOD_TYPE_S3M)
				{
					chn.nVibratoType = param & 0x03;
				} else
				{
					// IT compatibility: Ignore waveform types > 3
					if(m_playBehaviour[kITVibratoTremoloPanbrello])
						chn.nVibratoType = (param < 0x04) ? param : 0;
					else
						chn.nVibratoType = param & 0x07;
				}
				break;
	// S4x: Set Tremolo Waveform
	case 0x40:	if(GetType() == MOD_TYPE_S3M)
				{
					chn.nTremoloType = param & 0x03;
				} else
				{
					// IT compatibility: Ignore waveform types > 3
					if(m_playBehaviour[kITVibratoTremoloPanbrello])
						chn.nTremoloType = (param < 0x04) ? param : 0;
					else
						chn.nTremoloType = param & 0x07;
				}
				break;
	// S5x: Set Panbrello Waveform
	case 0x50:
		// IT compatibility: Ignore waveform types > 3
				if(m_playBehaviour[kITVibratoTremoloPanbrello])
				{
					chn.nPanbrelloType = (param < 0x04) ? param : 0;
					chn.nPanbrelloPos = 0;
				} else
				{
					chn.nPanbrelloType = param & 0x07;
				}
				break;
	// S6x: Pattern Delay for x frames
	case 0x60:
				if(m_PlayState.m_flags[SONG_FIRSTTICK] && m_PlayState.m_nTickCount == 0)
				{
					// Tick delays are added up.
					// Scream Tracker 3 does actually not support this command.
					// We'll use the same behaviour as for Impulse Tracker, as we can assume that
					// most S3Ms that make use of this command were made with Impulse Tracker.
					// MPT added this command to the XM format through the X6x effect, so we will use
					// the same behaviour here as well.
					// Test cases: PatternDelays.it, PatternDelays.s3m, PatternDelays.xm
					m_PlayState.m_nFrameDelay += param;
				}
				break;
	// S7x: Envelope Control / Instrument Control
	case 0x70:	if(!m_PlayState.m_flags[SONG_FIRSTTICK]) break;
				switch(param)
				{
				case 0:
				case 1:
				case 2:
					{
						for(CHANNELINDEX i = GetNumChannels(); i < m_PlayState.Chn.size(); i++)
						{
							ModChannel &bkChn = m_PlayState.Chn[i];
							if (bkChn.nMasterChn == nChn + 1)
							{
								if (param == 1)
								{
									KeyOff(bkChn);
									if(bkChn.dwFlags[CHN_ADLIB] && m_opl)
										m_opl->NoteOff(i);
								} else if (param == 2)
								{
									bkChn.dwFlags.set(CHN_NOTEFADE);
									if(bkChn.dwFlags[CHN_ADLIB] && m_opl)
										m_opl->NoteOff(i);
								} else
								{
									bkChn.dwFlags.set(CHN_NOTEFADE);
									bkChn.nFadeOutVol = 0;
									if(bkChn.dwFlags[CHN_ADLIB] && m_opl)
										m_opl->NoteCut(i);
								}
#ifndef NO_PLUGINS
								const ModInstrument *pIns = bkChn.pModInstrument;
								IMixPlugin *pPlugin;
								if(pIns != nullptr && pIns->nMixPlug && (pPlugin = m_MixPlugins[pIns->nMixPlug - 1].pMixPlugin) != nullptr)
								{
									pPlugin->MidiCommand(*pIns, bkChn.nNote | IMixPlugin::MIDI_NOTE_OFF, 0, m_playBehaviour[kLegacyPluginNNABehaviour] ? nChn : i);
								}
#endif // NO_PLUGINS
							}
						}
					}
					break;
				default:  // S73-S7E
					chn.InstrumentControl(param, *this);
					break;
				}
				break;
	// S8x: Set 4-bit Panning
	case 0x80:
		if(m_PlayState.m_flags[SONG_FIRSTTICK])
		{
			Panning(chn, param, Pan4bit);
		}
		break;
	// S9x: Sound Control
	case 0x90:
		if(m_PlayState.m_flags[SONG_FIRSTTICK])
		{
			ExtendedChannelEffect(chn, param, m_PlayState); break;
		}
		break;
	// SAx: Set 64k Offset
	case 0xA0:	if(m_PlayState.m_flags[SONG_FIRSTTICK])
				{
					chn.nOldHiOffset = static_cast<uint8>(param);
					if (!m_playBehaviour[kITHighOffsetNoRetrig] && chn.rowCommand.IsNote())
					{
						SmpLength pos = param << 16;
						if (pos < chn.nLength) chn.position.SetInt(pos);
					}
				}
				break;
	// SBx: Pattern Loop
	case 0xB0:
		if(m_PlayState.m_flags[SONG_FIRSTTICK])
			PatternLoop(m_PlayState, nChn, param & 0x0F);
		break;
	// SCx: Note Cut
	case 0xC0:
		if(param == 0)
		{
			//IT compatibility 22. SC0 == SC1
			if(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))
				param = 1;
			// ST3 doesn't cut notes with SC0
			else if(GetType() == MOD_TYPE_S3M)
				return;
		}
		// S3M/IT compatibility: Note Cut really cuts notes and does not just mute them (so that following volume commands could restore the sample)
		// Test case: scx.it
		NoteCut(nChn, param, m_playBehaviour[kITSCxStopsSample] || GetType() == MOD_TYPE_S3M);
		break;
	// SDx: Note Delay
	// SEx: Pattern Delay for x rows
	// SFx: S3M: Not used, IT: Set Active Midi Macro
	case 0xF0:
		if(GetType() != MOD_TYPE_S3M)
		{
			chn.nActiveMacro = static_cast<uint8>(param);
		}
		break;
	}
}


void CSoundFile::ExtendedChannelEffect(ModChannel &chn, uint32 param, PlayState &playState) const
{
	// S9x and X9x commands (S3M/XM/IT only)
	switch(param & 0x0F)
	{
	// S90: Surround Off
	case 0x00: chn.dwFlags.reset(CHN_SURROUND); break;
	// S91: Surround On
	case 0x01: chn.dwFlags.set(CHN_SURROUND); chn.nPan = 128; break;

	////////////////////////////////////////////////////////////
	// ModPlug Extensions
	// S98: Reverb Off
	case 0x08:
		chn.dwFlags.reset(CHN_REVERB);
		chn.dwFlags.set(CHN_NOREVERB);
		break;
	// S99: Reverb On
	case 0x09:
		chn.dwFlags.reset(CHN_NOREVERB);
		chn.dwFlags.set(CHN_REVERB);
		break;
	// S9A: 2-Channels surround mode
	case 0x0A:
		playState.m_flags.reset(SONG_SURROUNDPAN);
		break;
	// S9B: 4-Channels surround mode
	case 0x0B:
		playState.m_flags.set(SONG_SURROUNDPAN);
		break;
	// S9C: IT Filter Mode
	case 0x0C:
		playState.m_flags.reset(SONG_MPTFILTERMODE);
		break;
	// S9D: MPT Filter Mode
	case 0x0D:
		playState.m_flags.set(SONG_MPTFILTERMODE);
		break;
	// S9E: Go forward
	case 0x0E:
		chn.dwFlags.reset(CHN_PINGPONGFLAG);
		break;
	// S9F: Go backward (and set playback position to the end if sample just started)
	case 0x0F:
		if(chn.position.IsZero() && chn.nLength && (chn.rowCommand.IsNote() || !chn.dwFlags[CHN_LOOP]))
		{
			chn.position.Set(chn.nLength - 1, SamplePosition::fractMax);
		}
		chn.dwFlags.set(CHN_PINGPONGFLAG);
		break;
	}
}


void CSoundFile::InvertLoop(ModChannel &chn)
{
	// EFx implementation for MOD files (PT 1.1A and up: Invert Loop)
	// This effect trashes samples. Thanks to 8bitbubsy for making this work. :)
	if(GetType() != MOD_TYPE_MOD || chn.nEFxSpeed == 0)
		return;

	ModSample *pModSample = const_cast<ModSample *>(chn.pModSample);
	if(pModSample == nullptr || !pModSample->HasSampleData() || !pModSample->uFlags[CHN_LOOP | CHN_SUSTAINLOOP])
		return;

	chn.nEFxDelay += ModEFxTable[chn.nEFxSpeed & 0x0F];
	if(chn.nEFxDelay < 128)
		return;
	chn.nEFxDelay = 0;

	const SmpLength loopStart = pModSample->uFlags[CHN_LOOP] ? pModSample->nLoopStart : pModSample->nSustainStart;
	const SmpLength loopEnd = pModSample->uFlags[CHN_LOOP] ? pModSample->nLoopEnd : pModSample->nSustainEnd;

	if(++chn.nEFxOffset >= loopEnd - loopStart)
		chn.nEFxOffset = 0;

	// TRASH IT!!! (Yes, the sample!)
	const uint8 bps = pModSample->GetBytesPerSample();
	uint8 *begin = mpt::byte_cast<uint8 *>(pModSample->sampleb()) + (loopStart + chn.nEFxOffset) * bps;
	for(auto &sample : mpt::as_span(begin, bps))
	{
		sample = ~sample;
	}
	pModSample->PrecomputeLoops(*this, false);
}


// Process a MIDI Macro.
// Parameters:
// playState: The playback state to operate on.
// nChn: Mod channel to apply macro on
// isSmooth: If true, internal macros are interpolated between two rows
// macro: MIDI Macro string to process
// param: Parameter for parametric macros (Zxx / \xx parameter)
// plugin: Plugin to send MIDI message to (if not specified but needed, it is autodetected)
void CSoundFile::ProcessMIDIMacro(PlayState &playState, CHANNELINDEX nChn, bool isSmooth, const MIDIMacroConfigData::Macro &macro, uint8 param, PLUGINDEX plugin)
{
	playState.m_midiMacroScratchSpace.resize(macro.Length() + 1);
	MIDIMacroParser parser{*this, &playState, nChn, isSmooth, macro, mpt::as_span(playState.m_midiMacroScratchSpace), param, plugin};
	mpt::span<uint8> midiMsg;
	while(parser.NextMessage(midiMsg))
	{
		SendMIDIData(playState, nChn, isSmooth, midiMsg, plugin);
	}
}


// Calculate smooth MIDI macro slide parameter for current tick.
float CSoundFile::CalculateSmoothParamChange(const PlayState &playState, float currentValue, float param)
{
	MPT_ASSERT(playState.TicksOnRow() > playState.m_nTickCount);
	const uint32 ticksLeft = playState.TicksOnRow() - playState.m_nTickCount;
	if(ticksLeft > 1)
	{
		// Slide param
		const float step = (param - currentValue) / static_cast<float>(ticksLeft);
		return (currentValue + step);
	} else
	{
		// On last tick, set exact value.
		return param;
	}
}


// Process exactly one MIDI message parsed by ProcessMIDIMacro. Returns bytes sent on success, 0 on (parse) failure.
void CSoundFile::SendMIDIData(PlayState &playState, CHANNELINDEX nChn, bool isSmooth, const mpt::span<const uint8> macro, PLUGINDEX plugin)
{
	if(macro.size() < 1)
		return;

	// Don't do anything that modifies state outside of the playState itself.
	const bool localOnly = playState.m_midiMacroEvaluationResults.has_value();

	if(macro[0] == 0xFA || macro[0] == 0xFC || macro[0] == 0xFF)
	{
		// Start Song, Stop Song, MIDI Reset - both interpreted internally and sent to plugins
		for(CHANNELINDEX chn = 0; chn < GetNumChannels(); chn++)
		{
			playState.Chn[chn].nCutOff = 0x7F;
			playState.Chn[chn].nResonance = 0x00;
		}
	}

	ModChannel &chn = playState.Chn[nChn];
	if(macro.size() == 4 && macro[0] == 0xF0 && (macro[1] == 0xF0 || macro[1] == 0xF1))
	{
		// Internal device.
		const bool isExtended = (macro[1] == 0xF1);
		const uint8 macroCode = macro[2];
		const uint8 param = macro[3];

		if(macroCode == 0x00 && !isExtended && param < 0x80)
		{
			// F0.F0.00.xx: Set CutOff
			if(!isSmooth)
				chn.nCutOff = param;
			else
				chn.nCutOff = mpt::saturate_round<uint8>(CalculateSmoothParamChange(playState, chn.nCutOff, param));
			chn.nRestoreCutoffOnNewNote = 0;
			int cutoff = SetupChannelFilter(chn, !chn.dwFlags[CHN_FILTER]);

			if(cutoff >= 0 && chn.dwFlags[CHN_ADLIB] && m_opl && !localOnly)
			{
				// Cutoff doubles as modulator intensity for FM instruments
				m_opl->Volume(nChn, static_cast<uint8>(cutoff / 4), true);
			}
		} else if(macroCode == 0x01 && !isExtended && param < 0x80)
		{
			// F0.F0.01.xx: Set Resonance
			if(!isSmooth)
				chn.nResonance = param;
			else
				chn.nResonance = mpt::saturate_round<uint8>(CalculateSmoothParamChange(playState, chn.nResonance, param));
			chn.nRestoreResonanceOnNewNote = 0;
			SetupChannelFilter(chn, !chn.dwFlags[CHN_FILTER]);
		} else if(macroCode == 0x02 && !isExtended)
		{
			// F0.F0.02.xx: Set filter mode (high nibble determines filter mode)
			if(param < 0x20)
			{
				chn.nFilterMode = static_cast<FilterMode>(param >> 4);
				SetupChannelFilter(chn, !chn.dwFlags[CHN_FILTER]);
			}
#ifndef NO_PLUGINS
		} else if(macroCode == 0x03 && !isExtended)
		{
			// F0.F0.03.xx: Set plug dry/wet
			PLUGINDEX plug = (plugin != 0) ? plugin : GetBestPlugin(chn, nChn, PrioritiseChannel, EvenIfMuted);
			if(plug > 0 && plug <= MAX_MIXPLUGINS && param < 0x80)
			{
				plug--;
				if(IMixPlugin* pPlugin = m_MixPlugins[plug].pMixPlugin; pPlugin)
				{
					const float newRatio = (127 - param) / 127.0f;
					if(localOnly)
						playState.m_midiMacroEvaluationResults->pluginDryWetRatio[plug] = newRatio;
					else if(!isSmooth)
						pPlugin->SetDryRatio(newRatio);
					else
						pPlugin->SetDryRatio(CalculateSmoothParamChange(playState, m_MixPlugins[plug].fDryRatio, newRatio));
				}
			}
		} else if((macroCode & 0x80) || isExtended)
		{
			// F0.F0.{80|n}.xx / F0.F1.n.xx: Set VST effect parameter n to xx
			PLUGINDEX plug = (plugin != 0) ? plugin : GetBestPlugin(chn, nChn, PrioritiseChannel, EvenIfMuted);
			if(plug > 0 && plug <= MAX_MIXPLUGINS && param < 0x80)
			{
				plug--;
				if(IMixPlugin *pPlugin = m_MixPlugins[plug].pMixPlugin; pPlugin)
				{
					const PlugParamIndex plugParam = isExtended ? (0x80 + macroCode) : (macroCode & 0x7F);
					const PlugParamValue value = param / 127.0f;
					if(localOnly)
						playState.m_midiMacroEvaluationResults->pluginParameter[{plug, plugParam}] = value;
					else if(!isSmooth)
						pPlugin->SetParameter(plugParam, value, &playState, nChn);
					else
						pPlugin->SetParameter(plugParam, CalculateSmoothParamChange(playState, pPlugin->GetParameter(plugParam), value), &playState, nChn);
				}
			}
#endif // NO_PLUGINS
		}
	} else if(!localOnly)
	{
#ifndef NO_PLUGINS
		// Not an internal device. Pass on to appropriate plugin.
		const CHANNELINDEX plugChannel = (nChn < GetNumChannels()) ? nChn + 1 : chn.nMasterChn;
		if(plugChannel > 0 && plugChannel <= GetNumChannels())	// XXX do we need this? I guess it might be relevant for previewing notes in the pattern... Or when using this mechanism for volume/panning!
		{
			PLUGINDEX plug = 0;
			if(!chn.dwFlags[CHN_NOFX])
			{
				plug = (plugin != 0) ? plugin : GetBestPlugin(chn, nChn, PrioritiseChannel, EvenIfMuted);
			}

			if(plug > 0 && plug <= MAX_MIXPLUGINS)
			{
				if(IMixPlugin *pPlugin = m_MixPlugins[plug - 1].pMixPlugin; pPlugin != nullptr)
				{
					pPlugin->MidiSend(mpt::byte_cast<mpt::const_byte_span>(macro));
				}
			}
		}
#else
		MPT_UNREFERENCED_PARAMETER(plugin);
#endif // NO_PLUGINS
	}
}


void CSoundFile::SendMIDINote(CHANNELINDEX chn, uint16 note, uint16 volume, IMixPlugin *plugin)
{
#ifndef NO_PLUGINS
	auto &channel = m_PlayState.Chn[chn];
	const ModInstrument *pIns = channel.pModInstrument;
	// instro sends to a midi chan
	if(pIns && pIns->HasValidMIDIChannel())
	{
		if(plugin == nullptr && pIns->nMixPlug > 0 && pIns->nMixPlug <= MAX_MIXPLUGINS)
			plugin = m_MixPlugins[pIns->nMixPlug - 1].pMixPlugin;

		if(plugin != nullptr)
		{
			plugin->MidiCommand(*pIns, note, volume, chn);
			if(note < NOTE_MIN_SPECIAL)
				channel.nLeftVU = channel.nRightVU = 0xFF;
		}
	}
#endif // NO_PLUGINS
}


void CSoundFile::ProcessSampleOffset(ModChannel &chn, CHANNELINDEX nChn, const PlayState &playState) const
{
	const ModCommand &m = chn.rowCommand;
	uint32 extendedRows = 0;
	SmpLength offset = CalculateXParam(playState.m_nPattern, playState.m_nRow, nChn, &extendedRows), highOffset = 0;
	if(!extendedRows)
	{
		// No X-param (normal behaviour)
		const bool isPercentageOffset = (m.volcmd == VOLCMD_OFFSET && m.vol == 0);
		offset <<= 8;
		// FT2 compatibility: 9xx command without a note next to it does not update effect memory.
		// Test case: OffsetWithoutNote.xm
		if(offset && (!m_playBehaviour[kFT2OffsetMemoryRequiresNote] || m.IsNote()))
			chn.oldOffset = offset;
		else if(m.volcmd != VOLCMD_OFFSET)
			offset = chn.oldOffset;

		if(!isPercentageOffset)
			highOffset = static_cast<SmpLength>(chn.nOldHiOffset) << 16;
	}
	if(m.volcmd == VOLCMD_OFFSET)
	{
		if(m.vol == 0)
			offset = Util::muldivr_unsigned(chn.nLength, offset, 256u << (8u * std::max(uint32(1), extendedRows)));  // o00 + Oxx = Percentage Offset
		else if(m.vol <= std::size(ModSample().cues) && chn.pModSample != nullptr && !chn.pModSample->uFlags[CHN_ADLIB])
			offset += chn.pModSample->cues[m.vol - 1];  // Offset relative to cue point
		chn.oldOffset = offset;
	}
	SampleOffset(chn, offset + highOffset);
}


void CSoundFile::SampleOffset(ModChannel &chn, SmpLength param) const
{
	// ST3 compatibility: Instrument-less note recalls previous note's offset
	// Test case: OxxMemory.s3m
	if(m_playBehaviour[kST3OffsetWithoutInstrument] || GetType() == MOD_TYPE_MED)
		chn.prevNoteOffset = 0;
	
	chn.prevNoteOffset += param;

	if(param >= chn.nLoopEnd && (GetType() & (MOD_TYPE_S3M | MOD_TYPE_MTM)) && chn.dwFlags[CHN_LOOP] && chn.nLoopEnd > 0)
	{
		// Offset wrap-around
		// Note that ST3 only does this in GUS mode. SoundBlaster stops the sample entirely instead.
		// Test case: OffsetLoopWraparound.s3m
		param = (param - chn.nLoopStart) % (chn.nLoopEnd - chn.nLoopStart) + chn.nLoopStart;
	}

	if((GetType() & (MOD_TYPE_MDL | MOD_TYPE_PTM)) && chn.dwFlags[CHN_16BIT])
	{
		// Digitrakker and Polytracker use byte offsets, not sample offsets.
		param /= 2u;
	}

	// IT compatibility: Offset with instrument number but note note recalls previous note and executes offset.
	// Test case: OffsetWithInstr.it
	const auto note = (m_playBehaviour[kITOffsetWithInstrNumber] && chn.rowCommand.instr) ? chn.nNewNote : chn.rowCommand.note;
	if(ModCommand::IsNote(note) || m_playBehaviour[kApplyOffsetWithoutNote])
	{
		// IT compatibility: If this note is not mapped to a sample, ignore it.
		// Test case: empty_sample_offset.it
		if(chn.pModInstrument != nullptr && ModCommand::IsNote(note))
		{
			SAMPLEINDEX smp = chn.pModInstrument->Keyboard[note - NOTE_MIN];
			if(smp == 0 || smp > GetNumSamples())
				return;
		}

		if(m_SongFlags[SONG_PT_MODE])
		{
			// ProTracker compatibility: PT1/2-style funky 9xx offset command
			// Test case: ptoffset.mod
			chn.position.Set(chn.prevNoteOffset);
			chn.prevNoteOffset += param;
		} else
		{
			chn.position.Set(param);
		}

		if (chn.position.GetUInt() >= chn.nLength || (chn.dwFlags[CHN_LOOP] && chn.position.GetUInt() >= chn.nLoopEnd))
		{
			// Offset beyond sample size
			if(m_playBehaviour[kFT2ST3OffsetOutOfRange] || GetType() == MOD_TYPE_MTM)
			{
				// FT2 Compatibility: Don't play note if offset is beyond sample length
				// ST3 Compatibility: Don't play note if offset is beyond sample length (non-looped samples only)
				// Test cases: 3xx-no-old-samp.xm, OffsetPastSampleEnd.s3m
				chn.dwFlags.set(CHN_FASTVOLRAMP);
				chn.nPeriod = 0;
			} else if(!(GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2 | MOD_TYPE_MOD)))
			{
				// IT Compatibility: Offset
				if(m_playBehaviour[kITOffset])
				{
					if(m_SongFlags[SONG_ITOLDEFFECTS])
						chn.position.Set(chn.nLength); // Old FX: Clip to end of sample
					else
						chn.position.Set(0); // Reset to beginning of sample
				} else
				{
					chn.position.Set(chn.nLoopStart);
					if(m_SongFlags[SONG_ITOLDEFFECTS] && chn.nLength > 4)
					{
						chn.position.Set(chn.nLength - 2);
					}
				}
			} else if(GetType() == MOD_TYPE_MOD && chn.dwFlags[CHN_LOOP])
			{
				chn.position.Set(chn.nLoopStart);
			}
		}
	} else if ((param < chn.nLength) && (GetType() & (MOD_TYPE_MTM | MOD_TYPE_DMF | MOD_TYPE_MDL | MOD_TYPE_PLM)))
	{
		// Some trackers can also call offset effects without notes next to them...
		chn.position.Set(param);
	}
}


void CSoundFile::ReverseSampleOffset(ModChannel &chn, ModCommand::PARAM param) const
{
	if(chn.pModSample != nullptr && chn.pModSample->nLength > 0)
	{
		chn.dwFlags.set(CHN_PINGPONGFLAG);
		chn.dwFlags.reset(CHN_LOOP);
		chn.nLength = chn.pModSample->nLength;  // If there was a loop, extend sample to whole length.
		SmpLength offset = param << 8;
		if(GetType() == MOD_TYPE_PTM && chn.dwFlags[CHN_16BIT])
			offset /= 2;
		chn.position.Set((chn.nLength - 1) - std::min(offset, chn.nLength - SmpLength(1)), 0);
	}
}


void CSoundFile::DigiBoosterSampleReverse(ModChannel &chn, ModCommand::PARAM param) const
{
	if(chn.isFirstTick && chn.pModSample != nullptr && chn.pModSample->nLength > 0)
	{
		chn.dwFlags.set(CHN_PINGPONGFLAG);
		chn.nLength = chn.pModSample->nLength;  // If there was a loop, extend sample to whole length.
		chn.position.Set(chn.nLength - 1, 0);
		chn.dwFlags.set(CHN_LOOP | CHN_PINGPONGLOOP, param > 0);
		if(param > 0)
		{
			chn.nLoopStart = 0;
			chn.nLoopEnd = chn.nLength;
			// TODO: When the sample starts playing in forward direction again, the loop should be updated to the normal sample loop.
		}
	}
}


void CSoundFile::HandleDigiSamplePlayDirection(PlayState &state, CHANNELINDEX chn) const
{
	// Digi Booster mixes two channels into one Paula channel, and when a note is triggered on one of them it resets the reverse play flag on the other.
	if(GetType() == MOD_TYPE_DIGI)
	{
		state.Chn[chn].dwFlags.reset(CHN_PINGPONGFLAG);
		const CHANNELINDEX otherChn = chn ^ 1;
		if(otherChn < GetNumChannels())
			state.Chn[otherChn].dwFlags.reset(CHN_PINGPONGFLAG);
	}
}


void CSoundFile::RetrigNote(CHANNELINDEX nChn, int param, int offset)
{
	// Retrig: bit 8 is set if it's the new XM retrig
	ModChannel &chn = m_PlayState.Chn[nChn];
	int retrigSpeed = param & 0x0F;
	uint8 retrigCount = chn.nRetrigCount;
	bool doRetrig = false;

	// IT compatibility 15. Retrigger
	if(m_playBehaviour[kITRetrigger])
	{
		if(m_PlayState.m_nTickCount == 0 && chn.rowCommand.note)
		{
			chn.nRetrigCount = param & 0x0F;
		} else if(!chn.nRetrigCount || !--chn.nRetrigCount)
		{
			chn.nRetrigCount = param & 0x0F;
			doRetrig = true;
		}
	} else if(m_playBehaviour[kFT2Retrigger] && (param & 0x100))
	{
		// Buggy-like-hell FT2 Rxy retrig!
		// Test case: retrig.xm
		if(m_PlayState.m_flags[SONG_FIRSTTICK])
		{
			// Here are some really stupid things FT2 does on the first tick.
			// Test case: RetrigTick0.xm
			if(chn.rowCommand.instr > 0 && chn.rowCommand.IsNoteOrEmpty())
				retrigCount = 1;
			if(chn.rowCommand.volcmd == VOLCMD_VOLUME && chn.rowCommand.vol != 0)
			{
				// I guess this condition simply checked if the volume byte was != 0 in FT2.
				chn.nRetrigCount = retrigCount;
				return;
			}
		}
		if(retrigCount >= retrigSpeed)
		{
			if(!m_PlayState.m_flags[SONG_FIRSTTICK] || !chn.rowCommand.IsNote())
			{
				doRetrig = true;
				retrigCount = 0;
			}
		}
	} else
	{
		// old routines
		if (GetType() & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT))
		{
			if(!retrigSpeed)
				retrigSpeed = 1;
			if(retrigCount && !(retrigCount % retrigSpeed))
				doRetrig = true;
			retrigCount++;
		} else if(GetType() == MOD_TYPE_MOD)
		{
			// ProTracker-style retrigger
			// Test case: PTRetrigger.mod
			const auto tick = m_PlayState.m_nTickCount % m_PlayState.m_nMusicSpeed;
			if(!tick && chn.rowCommand.IsNote())
				return;
			if(retrigSpeed && !(tick % retrigSpeed))
				doRetrig = true;
		} else if(GetType() == MOD_TYPE_MTM)
		{
			// In MultiTracker, E9x retriggers the last note at exactly the x-th tick of the row
			doRetrig = m_PlayState.m_nTickCount == static_cast<uint32>(param & 0x0F) && retrigSpeed != 0;
		} else
		{
			int realspeed = retrigSpeed;
			// FT2 bug: if a retrig (Rxy) occurs together with a volume command, the first retrig interval is increased by one tick
			if((param & 0x100) && (chn.rowCommand.volcmd == VOLCMD_VOLUME) && (chn.rowCommand.param & 0xF0))
				realspeed++;
			if(!m_PlayState.m_flags[SONG_FIRSTTICK] || (param & 0x100))
			{
				if(!realspeed)
					realspeed = 1;
				if(!(param & 0x100) && m_PlayState.m_nMusicSpeed && !(m_PlayState.m_nTickCount % realspeed))
					doRetrig = true;
				retrigCount++;
			} else if(GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2))
				retrigCount = 0;
			if (retrigCount >= realspeed)
			{
				if(m_PlayState.m_nTickCount || ((param & 0x100) && !chn.rowCommand.note))
					doRetrig = true;
			}
			if(m_playBehaviour[kFT2Retrigger] && param == 0)
			{
				// E90 = Retrig instantly, and only once
				doRetrig = (m_PlayState.m_nTickCount == 0);
			}
		}
	}

	// IT compatibility: If a sample is shorter than the retrig time (i.e. it stops before the retrig counter hits zero), it is not retriggered.
	// Test case: retrig-short.it
	if(chn.nLength == 0 && m_playBehaviour[kITShortSampleRetrig] && !chn.HasMIDIOutput())
		return;
	// ST3 compatibility: No retrig after Note Cut
	// Test case: RetrigAfterNoteCut.s3m
	if(m_playBehaviour[kST3RetrigAfterNoteCut] && !chn.nFadeOutVol)
		return;

	if(doRetrig)
	{
		uint32 dv = (param >> 4) & 0x0F;
		int vol = chn.nVolume;
		if(dv)
		{

			// FT2 compatibility: Retrig + volume will not change volume of retrigged notes
			if(!m_playBehaviour[kFT2Retrigger] || !(chn.rowCommand.volcmd == VOLCMD_VOLUME))
			{
				if(retrigTable1[dv])
					vol = (vol * retrigTable1[dv]) / 16;
				else
					vol += ((int)retrigTable2[dv]) * 4;
			}
			Limit(vol, 0, 256);

			chn.dwFlags.set(CHN_FASTVOLRAMP);
		}
		uint32 note = chn.nNewNote;
		int32 oldPeriod = chn.nPeriod;
		// ST3 doesn't retrigger OPL notes
		// Test case: RetrigSlide.s3m
		const bool oplRealRetrig = chn.dwFlags[CHN_ADLIB] && m_playBehaviour[kOPLRealRetrig];
		if(note >= NOTE_MIN && note <= NOTE_MAX && chn.nLength && (GetType() != MOD_TYPE_S3M || oplRealRetrig))
			CheckNNA(nChn, 0, note, true);
		bool resetEnv = false;
		if(GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2))
		{
			if(chn.rowCommand.instr && param < 0x100)
			{
				InstrumentChange(chn, chn.rowCommand.instr, false, false);
				resetEnv = true;
			}
			if(param < 0x100)
				resetEnv = true;
		}
		// ProTracker Compatibility: Retrigger with lone instrument number causes instant sample change
		// Test case: InstrSwapRetrigger.mod
		if(m_playBehaviour[kMODSampleSwap] && chn.rowCommand.instr)
		{
			auto oldFineTune = chn.nFineTune;
			InstrumentChange(chn, chn.rowCommand.instr, false, false);
			chn.nFineTune = oldFineTune;
		}

		const bool fading = chn.dwFlags[CHN_NOTEFADE];
		const auto oldPrevNoteOffset = chn.prevNoteOffset;
		// Retriggered notes should not use previous offset in S3M
		// Test cases: OxxMemoryWithRetrig.s3m, PTOffsetRetrigger.mod
		if(GetType() == MOD_TYPE_S3M)
			chn.prevNoteOffset = 0;
		// IT compatibility: Really weird combination of envelopes and retrigger (see Storlek's q.it testcase)
		// Test cases: retrig.it, RetrigSlide.s3m
		const bool itS3Mstyle = m_playBehaviour[kITRetrigger] || (GetType() == MOD_TYPE_S3M && chn.nLength && !oplRealRetrig);
		NoteChange(chn, note, itS3Mstyle, resetEnv, false, nChn);
		if(!chn.rowCommand.instr)
			chn.prevNoteOffset = oldPrevNoteOffset;
		// XM compatibility: Prevent NoteChange from resetting the fade flag in case an instrument number + note-off is present.
		// Test case: RetrigFade.xm
		if(fading && GetType() == MOD_TYPE_XM)
			chn.dwFlags.set(CHN_NOTEFADE);
		chn.nVolume = vol;
		if(m_nInstruments)
		{
			chn.rowCommand.note = static_cast<ModCommand::NOTE>(note);	// No retrig without note...
#ifndef NO_PLUGINS
			ProcessMidiOut(nChn);	//Send retrig to Midi
#endif // NO_PLUGINS
		}
		if((GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) && chn.rowCommand.note == NOTE_NONE && oldPeriod != 0)
			chn.nPeriod = oldPeriod;
		if(!(GetType() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)))
			retrigCount = 0;
		// IT compatibility: see previous IT compatibility comment =)
		if(itS3Mstyle)
			chn.position.Set(0);

		offset--;
		if(chn.pModSample != nullptr && !chn.pModSample->uFlags[CHN_ADLIB] && offset >= 0 && offset <= static_cast<int>(std::size(chn.pModSample->cues)))
		{
			if(offset == 0)
				offset = chn.oldOffset;
			else
				offset = chn.oldOffset = chn.pModSample->cues[offset - 1];
			SampleOffset(chn, offset);
		}
	}

	// buggy-like-hell FT2 Rxy retrig!
	if(m_playBehaviour[kFT2Retrigger] && (param & 0x100))
		retrigCount++;

	// Now we can also store the retrig value for IT...
	if(!m_playBehaviour[kITRetrigger])
		chn.nRetrigCount = retrigCount;
}


// Execute a frequency slide on given channel.
// Positive amounts increase the frequency, negative amounts decrease it.
// The period or frequency that is read and written is in the period variable, chn.nPeriod is not touched.
void CSoundFile::DoFreqSlide(ModChannel &chn, int32 &period, int32 amount, bool isTonePorta) const
{
	if(!period || !amount)
		return;
	MPT_ASSERT(!chn.HasCustomTuning());

	if(GetType() == MOD_TYPE_669)
	{
		// Like other oldskool trackers, Composer 669 doesn't have linear slides...
		// But the slides are done in Hertz rather than periods, meaning that they
		// are more effective in the lower notes (rather than the higher notes).
		period += amount * 20;
	} else if(GetType() == MOD_TYPE_FAR)
	{
		period += (amount * 36318 / 1024);
	} else if(m_SongFlags[SONG_LINEARSLIDES] && !(GetType() & (MOD_TYPE_XM | MOD_TYPE_MOD)))
	{
		// IT Linear slides
		const auto oldPeriod = period;
		uint32 absAmount = std::abs(amount);

		// Note: IT ignores the lower 2 bits when abs(mount) > 16 (it either uses the fine *or* the regular table, not both)
		// This means that vibratos are slightly less accurate in this range than they could be.
		// Other code paths will *either* have an amount that's a multiple of 4 *or* it's less than 16.
		if(absAmount < 16)
		{
			if(amount > 0)
				period = Util::muldivr(period, GetFineLinearSlideUpTable(this, absAmount), 65536);
			else
				period = Util::muldivr(period, GetFineLinearSlideDownTable(this, absAmount), 65536);
		} else
		{
			absAmount /= 4u;
			while(absAmount > 0)
			{
				const uint32 n = std::min(absAmount, static_cast<uint32>(std::size(LinearSlideUpTable) - 1));
				if(amount > 0)
					period = Util::muldivr(period, GetLinearSlideUpTable(this, n), 65536);
				else
					period = Util::muldivr(period, GetLinearSlideDownTable(this, n), 65536);
				absAmount -= n;
			}
		}

		if(period == oldPeriod)
		{
			const bool incPeriod = m_playBehaviour[kPeriodsAreHertz] == (amount > 0);
			if(incPeriod && period < Util::MaxValueOfType(period))
				period++;
			else if(!incPeriod && period > 1)
				period--;
		}
	} else if(!m_SongFlags[SONG_LINEARSLIDES] && m_playBehaviour[kPeriodsAreHertz])
	{
		// IT Amiga slides
		if(amount < 0)
		{
			// Go down
			period = mpt::saturate_cast<int32>(Util::mul32to64_unsigned(1712 * 8363, period) / (Util::mul32to64_unsigned(period, -amount) + 1712 * 8363));
		} else if(amount > 0)
		{
			// Go up
			const auto periodDiv = 1712 * 8363 - Util::mul32to64(period, amount);
			if(periodDiv <= 0)
			{
				if(isTonePorta)
				{
					period = int32_max;
					return;
				} else
				{
					period = 0;
					chn.nFadeOutVol = 0;
					chn.dwFlags.set(CHN_NOTEFADE | CHN_FASTVOLRAMP);
				}
				return;
			}
			period = mpt::saturate_cast<int32>(Util::mul32to64_unsigned(1712 * 8363, period) / periodDiv);
		}
	} else
	{
		period -= amount;
	}
	if(period < 1)
	{
		period = 1;
		if(GetType() == MOD_TYPE_S3M && !isTonePorta)
		{
			chn.nFadeOutVol = 0;
			chn.dwFlags.set(CHN_NOTEFADE | CHN_FASTVOLRAMP);
		}
	}
}


void CSoundFile::NoteCut(CHANNELINDEX nChn, uint32 nTick, bool cutSample)
{
	if (m_PlayState.m_nTickCount == nTick)
	{
		ModChannel &chn = m_PlayState.Chn[nChn];
		if(cutSample)
		{
			chn.increment.Set(0);
			chn.nFadeOutVol = 0;
			chn.dwFlags.set(CHN_NOTEFADE);
		} else
		{
			chn.nVolume = 0;
		}
		chn.dwFlags.set(CHN_FASTVOLRAMP);

		// instro sends to a midi chan
		SendMIDINote(nChn, NOTE_KEYOFF, 0);
		
		if(chn.dwFlags[CHN_ADLIB] && m_opl)
		{
			m_opl->NoteCut(nChn, false);
		}
	}
}


void CSoundFile::KeyOff(ModChannel &chn) const
{
	const bool keyIsOn = !chn.dwFlags[CHN_KEYOFF];
	chn.dwFlags.set(CHN_KEYOFF);
	if(chn.pModInstrument != nullptr && !chn.VolEnv.flags[ENV_ENABLED])
	{
		chn.dwFlags.set(CHN_NOTEFADE);
	}
	if (!chn.nLength) return;
	if (chn.dwFlags[CHN_SUSTAINLOOP] && chn.pModSample && keyIsOn)
	{
		const ModSample *pSmp = chn.pModSample;
		if(pSmp->uFlags[CHN_LOOP])
		{
			if (pSmp->uFlags[CHN_PINGPONGLOOP])
				chn.dwFlags.set(CHN_PINGPONGLOOP);
			else
				chn.dwFlags.reset(CHN_PINGPONGLOOP | CHN_PINGPONGFLAG);
			chn.dwFlags.set(CHN_LOOP);
			chn.nLength = pSmp->nLength;
			chn.nLoopStart = pSmp->nLoopStart;
			chn.nLoopEnd = pSmp->nLoopEnd;
			if (chn.nLength > chn.nLoopEnd) chn.nLength = chn.nLoopEnd;
			if(chn.position.GetUInt() > chn.nLength)
			{
				// Test case: SusAfterLoop.it
				chn.position.Set(chn.nLoopStart + ((chn.position.GetInt() - chn.nLoopStart) % (chn.nLoopEnd - chn.nLoopStart)));
			}
		} else
		{
			chn.dwFlags.reset(CHN_LOOP | CHN_PINGPONGLOOP | CHN_PINGPONGFLAG);
			chn.nLength = pSmp->nLength;
		}
	}

	if (chn.pModInstrument)
	{
		const ModInstrument *pIns = chn.pModInstrument;
		if((pIns->VolEnv.dwFlags[ENV_LOOP] || (GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2 | MOD_TYPE_MDL))) && pIns->nFadeOut != 0)
		{
			chn.dwFlags.set(CHN_NOTEFADE);
		}

		if (pIns->VolEnv.nReleaseNode != ENV_RELEASE_NODE_UNSET && chn.VolEnv.nEnvValueAtReleaseJump == NOT_YET_RELEASED)
		{
			chn.VolEnv.nEnvValueAtReleaseJump = mpt::saturate_cast<int16>(pIns->VolEnv.GetValueFromPosition(chn.VolEnv.nEnvPosition, 256));
			chn.VolEnv.nEnvPosition = pIns->VolEnv[pIns->VolEnv.nReleaseNode].tick;
		}
	}
}


//////////////////////////////////////////////////////////
// CSoundFile: Global Effects


void CSoundFile::SetSpeed(PlayState &playState, uint32 param) const
{
#ifdef MODPLUG_TRACKER
	// FT2 appears to be decrementing the tick count before checking for zero,
	// so it effectively counts down 65536 ticks with speed = 0 (song speed is a 16-bit variable in FT2)
	if(GetType() == MOD_TYPE_XM && !param)
	{
		playState.m_nMusicSpeed = uint16_max;
	}
#endif	// MODPLUG_TRACKER
	if(param > 0) playState.m_nMusicSpeed = param;
	if(GetType() == MOD_TYPE_STM && param > 0)
	{
		playState.m_nMusicSpeed = std::max(param >> 4, uint32(1));
		playState.m_nMusicTempo = ConvertST2Tempo(static_cast<uint8>(param));
	}
}


// Convert a ST2 tempo byte to classic tempo and speed combination
TEMPO CSoundFile::ConvertST2Tempo(uint8 tempo)
{
	static constexpr uint8 ST2TempoFactor[] = { 140, 50, 25, 15, 10, 7, 6, 4, 3, 3, 2, 2, 2, 2, 1, 1 };
	static constexpr uint32 st2MixingRate = 23863; // Highest possible setting in ST2

	// This underflows at tempo 06...0F, and the resulting tick lengths depend on the mixing rate.
	// Note: ST2.3 uses the constant 50 below, earlier versions use 49 but they also play samples at a different speed.
	int32 samplesPerTick = st2MixingRate / (50 - ((ST2TempoFactor[tempo >> 4u] * (tempo & 0x0F)) >> 4u));
	if(samplesPerTick <= 0)
		samplesPerTick += 65536;
	return TEMPO().SetRaw(Util::muldivrfloor(st2MixingRate, 5 * TEMPO::fractFact, samplesPerTick * 2));
}


void CSoundFile::SetTempo(PlayState &playState, TEMPO param, bool setFromUI) const
{
	const CModSpecifications &specs = GetModSpecifications();

	// Anything lower than the minimum tempo is considered to be a tempo slide
	const TEMPO minTempo = GetMinimumTempoParam(GetType());
	TEMPO maxTempo = specs.GetTempoMax();
	// MED files may be imported with #xx parameter extension for tempos above 255, but they may be imported as either MOD or XM.
	// As regular MOD files cannot contain effect #xx, the tempo parameter cannot exceed 255 anyway, so we simply ignore their max tempo in CModSpecifications here.
	if(!(GetType() & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT)))
		maxTempo = GetModSpecifications(MOD_TYPE_MPT).GetTempoMax();
	if(m_playBehaviour[kTempoClamp])
		maxTempo.Set(255);

	if(setFromUI)
	{
		// Set tempo from UI - ignore slide commands and such.
		playState.m_nMusicTempo = Clamp(param, specs.GetTempoMin(), maxTempo);
	} else if(param >= minTempo && playState.m_flags[SONG_FIRSTTICK] == !m_playBehaviour[kMODTempoOnSecondTick])
	{
		// ProTracker sets the tempo after the first tick.
		// Note: The case of one tick per row is handled in ProcessRow() instead.
		// Test case: TempoChange.mod
		playState.m_nMusicTempo = std::min(param, maxTempo);
	} else if(param < minTempo && !playState.m_flags[SONG_FIRSTTICK])
	{
		// Tempo Slide
		TEMPO tempDiff(param.GetInt() & 0x0F, 0);
		if((param.GetInt() & 0xF0) == 0x10)
			playState.m_nMusicTempo += tempDiff;
		else
			playState.m_nMusicTempo -= tempDiff;

		TEMPO tempoMin = specs.GetTempoMin();
		Limit(playState.m_nMusicTempo, tempoMin, maxTempo);
	}
}


void CSoundFile::PatternLoop(PlayState &state, CHANNELINDEX nChn, ModCommand::PARAM param) const
{
	if(m_playBehaviour[kST3NoMutedChannels] && state.Chn[nChn].dwFlags[CHN_MUTE | CHN_SYNCMUTE])
		return;  // not even effects are processed on muted S3M channels

	// ST3 doesn't have per-channel pattern loop memory.
	ModChannel &chn = state.Chn[(GetType() == MOD_TYPE_S3M) ? 0 : nChn];

	if(!param)
	{
		// Loop Start
		chn.nPatternLoop = state.m_nRow;
		return;
	}

	// Loop Repeat
	if(chn.nPatternLoopCount)
	{
		// There's a loop left
		chn.nPatternLoopCount--;
		if(!chn.nPatternLoopCount)
		{
			// IT compatibility 10. Pattern loops (+ same fix for S3M files)
			// When finishing a pattern loop, the next loop without a dedicated SB0 starts on the first row after the previous loop.
			if(m_playBehaviour[kITPatternLoopTargetReset] || (GetType() == MOD_TYPE_S3M))
				chn.nPatternLoop = state.m_nRow + 1;

			return;
		}
	} else
	{
		// First time we get into the loop => Set loop count.

		// IT compatibility 10. Pattern loops (+ same fix for XM / MOD / S3M files)
		if(!m_playBehaviour[kITFT2PatternLoop] && !(GetType() & (MOD_TYPE_MOD | MOD_TYPE_S3M)))
		{
			for(const ModChannel &otherChn : state.PatternChannels(*this))
			{
				// Loop on other channel
				if(&otherChn != &chn && otherChn.nPatternLoopCount)
					return;
			}
		}
		chn.nPatternLoopCount = param;
	}
	state.m_nextPatStartRow = chn.nPatternLoop;  // Nasty FT2 E60 bug emulation!

	const auto loopTarget = chn.nPatternLoop;
	if(loopTarget != ROWINDEX_INVALID)
	{
		// FT2 compatibility: E6x overwrites jump targets of Dxx effects that are located left of the E6x effect.
		// Test cases: PatLoop-Jumps.xm, PatLoop-Various.xm
		if(state.m_breakRow != ROWINDEX_INVALID && m_playBehaviour[kFT2PatternLoopWithJumps])
			state.m_breakRow = loopTarget;

		state.m_patLoopRow = loopTarget;
		// IT compatibility: SBx is prioritized over Position Jump (Bxx) effects that are located left of the SBx effect.
		// Test case: sbx-priority.it, LoopBreak.it
		if(m_playBehaviour[kITPatternLoopWithJumps])
			state.m_posJump = ORDERINDEX_INVALID;
	}
}


void CSoundFile::GlobalVolSlide(PlayState &playState, ModCommand::PARAM param, CHANNELINDEX chn) const
{
	if(m_SongFlags[SONG_AUTO_GLOBALVOL])
		playState.Chn[chn].autoSlide.SetActive(AutoSlideCommand::GlobalVolumeSlide, param != 0);

	if(param)
		playState.Chn[chn].nOldGlobalVolSlide = param;
	else
		param = playState.Chn[chn].nOldGlobalVolSlide;

	if((GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2)))
	{
		// XM nibble priority
		if((param & 0xF0) != 0)
		{
			param &= 0xF0;
		} else
		{
			param &= 0x0F;
		}
	}

	int32 nGlbSlide = 0;
	if (((param & 0x0F) == 0x0F) && (param & 0xF0))
	{
		if(playState.m_flags[SONG_FIRSTTICK]) nGlbSlide = (param >> 4) * 2;
	} else
	if (((param & 0xF0) == 0xF0) && (param & 0x0F))
	{
		if(playState.m_flags[SONG_FIRSTTICK]) nGlbSlide = - (int)((param & 0x0F) * 2);
	} else
	{
		if(!playState.m_flags[SONG_FIRSTTICK])
		{
			if (param & 0xF0)
			{
				// IT compatibility: Ignore slide commands with both nibbles set.
				if(!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_IMF | MOD_TYPE_J2B | MOD_TYPE_MID | MOD_TYPE_AMS | MOD_TYPE_DBM)) || (param & 0x0F) == 0)
					nGlbSlide = (int)((param & 0xF0) >> 4) * 2;
			} else
			{
				nGlbSlide = -(int)((param & 0x0F) * 2);
			}
		}
	}
	if (nGlbSlide)
	{
		if(!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_IMF | MOD_TYPE_J2B | MOD_TYPE_MID | MOD_TYPE_AMS | MOD_TYPE_DBM))) nGlbSlide *= 2;
		nGlbSlide += playState.m_nGlobalVolume;
		Limit(nGlbSlide, 0, 256);
		playState.m_nGlobalVolume = nGlbSlide;
	}
}


//////////////////////////////////////////////////////
// Note/Period/Frequency functions

// Find lowest note which has same or lower period as a given period (i.e. the note has the same or higher frequency)
uint32 CSoundFile::GetNoteFromPeriod(uint32 period, int32 nFineTune, uint32 nC5Speed) const
{
	if(!period) return 0;
	if(m_playBehaviour[kFT2Periods])
	{
		// FT2's "RelocateTon" function actually rounds up and down, while GetNoteFromPeriod normally just truncates.
		nFineTune += 64;
	}
	// This essentially implements std::lower_bound, with the difference that we don't need an iterable container.
	uint32 minNote = NOTE_MIN, maxNote = NOTE_MAX, count = maxNote - minNote + 1;
	const bool periodIsFreq = PeriodsAreFrequencies();
	while(count > 0)
	{
		const uint32 step = count / 2, midNote = minNote + step;
		uint32 n = GetPeriodFromNote(midNote, nFineTune, nC5Speed);
		if((n > period && !periodIsFreq) || (n < period && periodIsFreq) || !n)
		{
			minNote = midNote + 1;
			count -= step + 1;
		} else
		{
			count = step;
		}
	}
	return minNote;
}


uint32 CSoundFile::GetPeriodFromNote(uint32 note, int32 nFineTune, uint32 nC5Speed) const
{
	if (note == NOTE_NONE || (note >= NOTE_MIN_SPECIAL)) return 0;
	note -= NOTE_MIN;
	if(!UseFinetuneAndTranspose())
	{
		if(GetType() == MOD_TYPE_MDL)
		{
			// MDL uses non-linear slides, but their effectiveness does not depend on the middle-C frequency.
			MPT_ASSERT(!PeriodsAreFrequencies());
			return (FreqS3MTable[note % 12u] << 4) >> (note / 12);
		} else if(GetType() == MOD_TYPE_DTM)
		{
			// Similar to MDL, but finetune is factored in and we don't transpose everything by an octave
			MPT_ASSERT(!PeriodsAreFrequencies());
			return (ProTrackerTunedPeriods[XM2MODFineTune(nFineTune) * 12u + note % 12u] << 5) >> (note / 12u);
		}
		if(!nC5Speed)
			nC5Speed = 8363;
		if(PeriodsAreFrequencies())
		{
			// Compute everything in Hertz rather than periods.
			uint32 freq = Util::muldiv_unsigned(nC5Speed, LinearSlideUpTable[(note % 12u) * 16u] << (note / 12u), 65536 << 5);
			LimitMax(freq, static_cast<uint32>(int32_max));
			return freq;
		} else if(m_SongFlags[SONG_LINEARSLIDES])
		{
			return (FreqS3MTable[note % 12u] << 5) >> (note / 12);
		} else
		{
			LimitMax(nC5Speed, uint32_max >> (note / 12u));
			//(a*b)/c
			return Util::muldiv_unsigned(8363, (FreqS3MTable[note % 12u] << 5), nC5Speed << (note / 12u));
			//8363 * freq[note%12] / nC5Speed * 2^(5-note/12)
		}
	} else if((GetType() & (MOD_TYPE_XM | MOD_TYPE_MTM)) || (m_SongFlags[SONG_LINEARSLIDES] && UseFinetuneAndTranspose()))
	{
		if (note < 12) note = 12;
		note -= 12;

		if(GetType() == MOD_TYPE_MTM)
		{
			nFineTune *= 16;
		} else if(m_playBehaviour[kFT2FinetunePrecision])
		{
			// FT2 Compatibility: The lower three bits of the finetune are truncated.
			// Test case: Finetune-Precision.xm
			nFineTune &= ~7;
		}

		if(m_SongFlags[SONG_LINEARSLIDES])
		{
			int l = ((120 - note) << 6) - (nFineTune / 2);
			if (l < 1) l = 1;
			return static_cast<uint32>(l);
		} else
		{
			int finetune = nFineTune;
			uint32 rnote = (note % 12) << 3;
			uint32 roct = note / 12;
			int rfine = finetune / 16;
			int i = rnote + rfine + 8;
			Limit(i , 0, 103);
			uint32 per1 = XMPeriodTable[i];
			if(finetune < 0)
			{
				rfine--;
				finetune = -finetune;
			} else rfine++;
			i = rnote+rfine+8;
			if (i < 0) i = 0;
			if (i >= 104) i = 103;
			uint32 per2 = XMPeriodTable[i];
			rfine = finetune & 0x0F;
			per1 *= 16-rfine;
			per2 *= rfine;
			return ((per1 + per2) << 1) >> roct;
		}
	} else
	{
		nFineTune = XM2MODFineTune(nFineTune);
		if ((nFineTune) || (note < 24) || (note >= 24 + std::size(ProTrackerPeriodTable)))
			return (ProTrackerTunedPeriods[nFineTune * 12u + note % 12u] << 5) >> (note / 12u);
		else
			return (ProTrackerPeriodTable[note - 24] << 2);
	}
}


// Converts period value to sample frequency. Return value is fixed point, with FREQ_FRACBITS fractional bits.
uint32 CSoundFile::GetFreqFromPeriod(uint32 period, uint32 c5speed, int32 nPeriodFrac) const
{
	if (!period) return 0;
	if ((GetType() & (MOD_TYPE_XM | MOD_TYPE_MTM)) || (m_SongFlags[SONG_LINEARSLIDES] && UseFinetuneAndTranspose()))
	{
		if(m_playBehaviour[kFT2Periods])
		{
			// FT2 compatibility: Period is a 16-bit value in FT2, and it overflows happily.
			// Test case: FreqWraparound.xm
			period &= 0xFFFF;
		}
		if(m_SongFlags[SONG_LINEARSLIDES])
		{
			uint32 octave;
			if(m_playBehaviour[kFT2Periods])
			{
				// Under normal circumstances, this calculation returns the same values as the non-compatible one.
				// However, once the 12 octaves are exceeded (through portamento slides), the octave shift goes
				// crazy in FT2, meaning that the frequency wraps around randomly...
				// The entries in FT2's conversion table are four times as big, hence we have to do an additional shift by two bits.
				// Test case: FreqWraparound.xm
				// 12 octaves * (12 * 64) LUT entries = 9216, add 767 for rounding
				uint32 div = ((9216u + 767u - period) / 768);
				octave = ((14 - div) & 0x1F);
			} else
			{
				if(period > 29 * 768)
					return 0;
				octave = (period / 768) + 2;
			}
			return (XMLinearTable[period % 768] << (FREQ_FRACBITS + 2)) >> octave;
		} else
		{
			if(!period) period = 1;
			return ((8363 * 1712L) << FREQ_FRACBITS) / period;
		}
	} else if(UseFinetuneAndTranspose())
	{
		return ((3546895L * 4) << FREQ_FRACBITS) / period;
	} else if(GetType() == MOD_TYPE_669)
	{
		// We only really use c5speed for the finetune pattern command. All samples in 669 files have the same middle-C speed (imported as 8363 Hz).
		return (period + c5speed - 8363) << FREQ_FRACBITS;
	} else if(GetType() == MOD_TYPE_MDL)
	{
		MPT_ASSERT(!PeriodsAreFrequencies());
		LimitMax(period, Util::MaxValueOfType(period) >> 8);
		if (!c5speed) c5speed = 8363;
		return Util::muldiv_unsigned(c5speed, (1712L << 7) << FREQ_FRACBITS, (period << 8) + nPeriodFrac);
	} else
	{
		LimitMax(period, Util::MaxValueOfType(period) >> 8);
		if(PeriodsAreFrequencies())
		{
			// Input is already a frequency in Hertz, not a period.
			static_assert(FREQ_FRACBITS <= 8, "Check this shift operator");
			return uint32(((uint64(period) << 8) + nPeriodFrac) >> (8 - FREQ_FRACBITS));
		} else if(m_SongFlags[SONG_LINEARSLIDES] || GetType() == MOD_TYPE_DTM)
		{
			if(!c5speed)
				c5speed = 8363;
			return Util::muldiv_unsigned(c5speed, (1712L << 8) << FREQ_FRACBITS, (period << 8) + nPeriodFrac);
		} else
		{
			return Util::muldiv_unsigned(8363, (1712L << 8) << FREQ_FRACBITS, (period << 8) + nPeriodFrac);
		}
	}
}


PLUGINDEX CSoundFile::GetBestPlugin(const ModChannel &channel, CHANNELINDEX nChn, PluginPriority priority, PluginMutePriority respectMutes) const
{
	//Define search source order
	PLUGINDEX plugin = 0;
	switch(priority)
	{
		case ChannelOnly:
			plugin = GetChannelPlugin(channel, nChn, respectMutes);
			break;
		case InstrumentOnly:
			plugin  = GetActiveInstrumentPlugin(channel, respectMutes);
			break;
		case PrioritiseInstrument:
			plugin  = GetActiveInstrumentPlugin(channel, respectMutes);
			if(!plugin || plugin > MAX_MIXPLUGINS)
			{
				plugin = GetChannelPlugin(channel, nChn, respectMutes);
			}
			break;
		case PrioritiseChannel:
			plugin  = GetChannelPlugin(channel, nChn, respectMutes);
			if(!plugin || plugin > MAX_MIXPLUGINS)
			{
				plugin = GetActiveInstrumentPlugin(channel, respectMutes);
			}
			break;
	}

	return plugin; // 0 Means no plugin found.
}


PLUGINDEX CSoundFile::GetChannelPlugin(const ModChannel &channel, CHANNELINDEX nChn, PluginMutePriority respectMutes) const
{
	PLUGINDEX plugin;
	if((respectMutes == RespectMutes && channel.dwFlags[CHN_MUTE | CHN_SYNCMUTE]) || channel.dwFlags[CHN_NOFX])
	{
		plugin = 0;
	} else
	{
		// If it looks like this is an NNA channel, we need to find the master channel.
		// This ensures we pick up the right ChnSettings.
		if(channel.nMasterChn > 0)
			nChn = channel.nMasterChn - 1;

		if(nChn < ChnSettings.size())
			plugin = ChnSettings[nChn].nMixPlugin;
		else
			plugin = 0;
	}
	return plugin;
}


PLUGINDEX CSoundFile::GetActiveInstrumentPlugin(const ModChannel &chn, PluginMutePriority respectMutes)
{
	// Unlike channel settings, pModInstrument is copied from the original chan to the NNA chan,
	// so we don't need to worry about finding the master chan.

	PLUGINDEX plug = 0;
	if(chn.pModInstrument != nullptr)
	{
		if(respectMutes == RespectMutes && chn.pModInstrument->dwFlags[INS_MUTE])
			plug = 0;
		else
			plug = chn.pModInstrument->nMixPlug;
	}
	return plug;
}


// Retrieve the plugin that is associated with the channel's current instrument.
// No plugin is returned if the channel is muted or if the instrument doesn't have a MIDI channel set up,
// As this is meant to be used with instrument plugins.
IMixPlugin *CSoundFile::GetChannelInstrumentPlugin(const ModChannel &chn) const
{
#ifndef NO_PLUGINS
	if(chn.dwFlags[CHN_MUTE | CHN_SYNCMUTE])
	{
		// Don't process portamento on muted channels. Note that this might have a side-effect
		// on other channels which trigger notes on the same MIDI channel of the same plugin,
		// as those won't be pitch-bent anymore.
		return nullptr;
	}

	if(chn.HasMIDIOutput())
	{
		const ModInstrument *pIns = chn.pModInstrument;
		// Instrument sends to a MIDI channel
		if(pIns->nMixPlug != 0 && pIns->nMixPlug <= MAX_MIXPLUGINS)
		{
			return m_MixPlugins[pIns->nMixPlug - 1].pMixPlugin;
		}
	}
#else
	MPT_UNREFERENCED_PARAMETER(chn);
#endif // NO_PLUGINS
	return nullptr;
}


#ifdef MODPLUG_TRACKER
void CSoundFile::HandleRowTransitionEvents(bool nextPattern)
{
	bool doTransition = nextPattern;

	// Jump to another pattern?
	if(m_PlayState.m_nSeqOverride != ORDERINDEX_INVALID && m_PlayState.m_nSeqOverride < Order().size())
	{
		switch(m_PlayState.m_seqOverrideMode)
		{
		case OrderTransitionMode::AtPatternEnd:
			doTransition = nextPattern;
			break;
		case OrderTransitionMode::AtMeasureEnd:
			if(m_PlayState.m_nCurrentRowsPerMeasure > 0)
				doTransition = (m_PlayState.m_nRow % m_PlayState.m_nCurrentRowsPerMeasure) == 0;
			break;
		case OrderTransitionMode::AtBeatEnd:
			if(m_PlayState.m_nCurrentRowsPerBeat > 0)
				doTransition = (m_PlayState.m_nRow % m_PlayState.m_nCurrentRowsPerBeat) == 0;
			break;
		case OrderTransitionMode::AtRowEnd:
			doTransition = true;
			break;
		}
		if(doTransition)
		{
			if(m_PlayState.m_flags[SONG_PATTERNLOOP])
				m_PlayState.m_nPattern = Order()[m_PlayState.m_nSeqOverride];
			m_PlayState.m_nCurrentOrder = m_PlayState.m_nSeqOverride;
			m_PlayState.m_nSeqOverride = ORDERINDEX_INVALID;
		}
	}

	if(doTransition && GetpModDoc())
	{
		// Update channel mutes
		for(CHANNELINDEX chan = 0; chan < GetNumChannels(); chan++)
		{
			if(m_bChannelMuteTogglePending[chan])
			{
				GetpModDoc()->MuteChannel(chan, !GetpModDoc()->IsChannelMuted(chan));
				m_bChannelMuteTogglePending[chan] = false;
			}
		}
	}

	// Metronome
	if(IsMetronomeEnabled() && !IsRenderingToDisc() && !m_PlayState.m_flags[SONG_PAUSED | SONG_STEP])
	{
		const ROWINDEX rpm = m_PlayState.m_nCurrentRowsPerMeasure ? m_PlayState.m_nCurrentRowsPerMeasure : DEFAULT_ROWS_PER_MEASURE;
		const ROWINDEX rpb = m_PlayState.m_nCurrentRowsPerBeat ? m_PlayState.m_nCurrentRowsPerBeat : DEFAULT_ROWS_PER_BEAT;
		const ModSample *sample = nullptr;
		if(!m_PlayState.m_lTotalSampleCount || !(m_PlayState.m_nRow % rpm))
			sample = m_metronomeMeasure;
		else if(!(m_PlayState.m_nRow % rpm % rpb))
			sample = m_metronomeBeat;
		if(sample)
		{
			m_metronomeChn.pModSample = sample;
			m_metronomeChn.pCurrentSample = sample->samplev();
			m_metronomeChn.dwFlags = (sample->uFlags & CHN_SAMPLEFLAGS) | CHN_NOREVERB;
			m_metronomeChn.position.Set(0);
			m_metronomeChn.increment = SamplePosition::Ratio(sample->nC5Speed, m_MixerSettings.gdwMixingFreq);
			m_metronomeChn.rampLeftVol = m_metronomeChn.rampRightVol = m_metronomeChn.leftVol = m_metronomeChn.rightVol = sample->nVolume * 16;
			m_metronomeChn.leftRamp = m_metronomeChn.rightRamp = 0;
			m_metronomeChn.nLength = m_metronomeChn.pModSample->nLength;
			m_metronomeChn.resamplingMode = m_Resampler.m_Settings.SrcMode;
		}
	} else
	{
		m_metronomeChn.pCurrentSample = nullptr;
	}
}
#endif // MODPLUG_TRACKER


void CSoundFile::PortamentoMPT(ModChannel &chn, int param) const
{
	//Behavior: Modifies portamento by param-steps on every tick.
	//Note that step meaning depends on tuning.

	chn.m_PortamentoFineSteps += param;
	chn.m_CalculateFreq = true;
}


void CSoundFile::PortamentoFineMPT(PlayState &playState, CHANNELINDEX nChn, int param) const
{
	ModChannel &chn = playState.Chn[nChn];
	//Behavior: Divides portamento change between ticks/row. For example
	//if Ticks/row == 6, and param == +-6, portamento goes up/down by one tuning-dependent
	//fine step every tick.

	if(playState.m_nTickCount == 0)
		chn.nOldFinePortaUpDown = 0;

	const int tickParam = static_cast<int>((playState.m_nTickCount + 1.0) * param / playState.m_nMusicSpeed);
	chn.m_PortamentoFineSteps += (param >= 0) ? tickParam - chn.nOldFinePortaUpDown : tickParam + chn.nOldFinePortaUpDown;
	if(playState.m_nTickCount + 1 == playState.m_nMusicSpeed)
		chn.nOldFinePortaUpDown = static_cast<int8>(std::abs(param));
	else
		chn.nOldFinePortaUpDown = static_cast<int8>(std::abs(tickParam));

	chn.m_CalculateFreq = true;
}


void CSoundFile::PortamentoExtraFineMPT(ModChannel &chn, int param) const
{
	// This kinda behaves like regular fine portamento.
	// It changes the pitch by n finetune steps on the first tick.

	if(chn.isFirstTick)
	{
		chn.m_PortamentoFineSteps += param;
		chn.m_CalculateFreq = true;
	}
}


OPENMPT_NAMESPACE_END
