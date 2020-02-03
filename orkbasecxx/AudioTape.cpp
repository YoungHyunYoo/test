/*
 * Oreka -- A media capture and retrieval platform
 * 
 * Copyright (C) 2005, orecx LLC
 *
 * http://www.orecx.com
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License.
 * Please refer to http://www.gnu.org/copyleft/gpl.html
 *
 */
#pragma warning( disable: 4786 )

#define _WINSOCKAPI_		// prevents the inclusion of winsock.h


#include "ace/OS_NS_time.h"
#include "Utils.h"
#include "ThreadSafeQueue.h"
#include "LogManager.h"
#include "audiofile/PcmFile.h"
#include "audiofile/LibSndFileFile.h"
#include "audiofile/MediaChunkFile.h"
#include "audiofile/SttFile.h"
#include "messages/TapeMsg.h"
#include "AudioTape.h"
#include "ConfigManager.h"
#include "PartyFilter.h"
#include "ConfigManager.h"

AudioTapeDescription::AudioTapeDescription()
{
	m_direction = CaptureEvent::DirUnkn;
	m_localSide = CaptureEvent::LocalSideUnkn;

	if(CaptureEvent::AudioKeepDirectionIsDefault(CONFIG.m_audioKeepDirectionIncomingDefault) == false)
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionIncomingDefault);
	}
	else if(CaptureEvent::AudioKeepDirectionIsDefault(CONFIG.m_audioKeepDirectionOutgoingDefault) == false)
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionOutgoingDefault);
	}
	else
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionDefault);
	}

	m_duration = 0;
	m_beginDate = 0;
}

void AudioTapeDescription::Define(Serializer* s)
{
	s->DateValue("date", m_beginDate);
	s->IntValue("duration", m_duration);
	s->EnumValue("direction", (int&)m_direction, CaptureEvent::DirectionToEnum, CaptureEvent::DirectionToString);
	s->EnumValue("localside", (int&)m_localSide, CaptureEvent::LocalSideToEnum, CaptureEvent::LocalSideToString);
	s->EnumValue("audiokeepdirection", (int&)m_audioKeepDirectionEnum, CaptureEvent::AudioKeepDirectionToEnum, CaptureEvent::AudioKeepDirectionToString);
	s->StringValue("capturePort", m_capturePort);
	s->StringValue("localParty", m_localParty);
	s->StringValue("remoteParty", m_remoteParty);
	s->StringValue("localEntryPoint", m_localEntryPoint);
	s->StringValue("localIp", m_localIp);
	s->StringValue("remoteIp", m_remoteIp);
	s->StringValue("filename", m_filename);
	s->BoolValue("ondemand", m_onDemand);
}

void AudioTapeDescription::Validate(){}

CStdString AudioTapeDescription::GetClassName()
{
	return "tapedescription";
}

ObjectRef AudioTapeDescription::NewInstance()
{
	ObjectRef ref(new AudioTapeDescription());
	return ref;
}

ObjectRef AudioTapeDescription::Process() 
{
	ObjectRef ref;
	return ref;
}


//===================================================
AudioTape::AudioTape(CStdString &portId)
{
//printf("__AudioTape::AudioTape(port)\n");
	m_portId = portId;
	m_state = StateCreated;
	m_sttState = StateCreated;
	m_beginDate = ACE_OS::time(NULL);
	m_endDate = 0;
	m_duration = 0;
	m_direction = CaptureEvent::DirUnkn;
	m_localSide = CaptureEvent::LocalSideUnkn;
	m_isExternal = false;
	m_mediaType = MediaType::AudioType; 	// by default, the tape should be audio
	m_isSuccessfulImported = true;
	m_isDoneProcessed = false;

	if(CaptureEvent::AudioKeepDirectionIsDefault(CONFIG.m_audioKeepDirectionIncomingDefault) == false)
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionIncomingDefault);
	}
	else if(CaptureEvent::AudioKeepDirectionIsDefault(CONFIG.m_audioKeepDirectionOutgoingDefault) == false)
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionOutgoingDefault);
	}
	else
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionDefault);
	}

	m_shouldStop = false;
	m_sttShouldStop = false;
	m_readyForBatchProcessing = false;
	m_trackingId = portId;	// to make sure this has a value before we get the capture tracking Id.
	m_bytesWritten = 0;
	m_lastLogWarning = 0;
	m_passedPartyFilterTest = false;
	m_numErrors = 0;
	m_onDemand = false;
	m_pushCount = 0;
	m_popCount = 0;
	m_highMark = 0;
	m_chunkQueueDataSize = 0;
	m_chunkQueueErrorReported = false;
	m_keep = true;


	m_sttPushCount=0;
	m_sttPopCount=0;
	m_sttHighMark=0;
	m_sttChunkQueueDataSize =0;
//	m_sttEnable=true;
	m_sttReadyForBatchProcessing=false;
//	m_sttEnable=false;
	//m_sttChunkQueueErrorReported = false;


	m_audioOutputPath = CONFIG.m_audioOutputPathMcf;

	GenerateCaptureFilePathAndIdentifier();
}

AudioTape::AudioTape(CStdString &portId, CStdString& file)
{
	m_passedPartyFilterTest = false;
	m_portId = portId;
	m_onDemand = false;
	m_localSide = CaptureEvent::LocalSideUnkn;

	if(CaptureEvent::AudioKeepDirectionIsDefault(CONFIG.m_audioKeepDirectionIncomingDefault) == false)
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionIncomingDefault);
		}
	else if(CaptureEvent::AudioKeepDirectionIsDefault(CONFIG.m_audioKeepDirectionOutgoingDefault) == false)
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionOutgoingDefault);
	}
	else
	{
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(CONFIG.m_audioKeepDirectionDefault);
	}

	m_chunkQueueDataSize = 0;
	m_chunkQueueErrorReported = false;


	// Extract Path and Identifier
	m_filePath = FilePath(file);
	if(m_filePath.Find(CONFIG.m_audioOutputPath) >= 0) {
		m_filePath = (m_filePath.Right(m_filePath.size() - 1 - CONFIG.m_audioOutputPath.size()));
	}

	CStdString basename = FileBaseName(file);
	m_fileIdentifier = FileStripExtension(basename);

	// Create the audiofile
	m_audioFileRef.reset(new MediaChunkFile());
	m_audioFileRef->SetFilename(file);
printf("\n\n+++++++++++\n");
	m_audioSttFileRef.reset(new SttFile());
printf("\n\n-----------\n");
}

void AudioTape::AddAudioChunk(AudioChunkRef chunkRef)
{
	// Add the chunk to the local queue
	if(m_state == StateCreated || m_state == StateActive)
	{
		MutexSentinel sentinel(m_mutex);

		if(m_chunkQueueDataSize >= (CONFIG.m_captureFileBatchSizeKByte * 3 * 1024))
		{

			if(m_chunkQueueErrorReported == false)
			{
				m_chunkQueueErrorReported = true;
				LOG4CXX_ERROR(LOG.tapeLog, "Rejected additional chunk due to slow hard drive -- Queued Data Size:" + FormatDataSize(m_chunkQueueDataSize) + " is greater than 3*CaptureFileBatchSizeKByte:" + FormatDataSize(CONFIG.m_captureFileBatchSizeKByte * 3 * 1024));
			}

			return;
		}
		
		if(CONFIG.m_sttEnable==true)
		{
			m_sttChunkQueue.push(chunkRef);
			m_sttChunkQueueDataSize += chunkRef->GetNumBytes();
			m_sttPushCount+=1;
			if(m_sttChunkQueue.size()>m_sttHighMark)
			{
				m_sttHighMark = m_sttChunkQueue.size();
			}
		}



		m_chunkQueue.push(chunkRef);
		m_chunkQueueDataSize += chunkRef->GetNumBytes();
		m_pushCount += 1;

		if(m_chunkQueue.size() > m_highMark)
		{
			m_highMark = m_chunkQueue.size();
		}

	}

}




void AudioTape::WriteStt()
{
        bool done = false;
        CStdString logMsg;

        while(!done && m_sttState != StateStopped && m_sttState != StateError) 
        {
                // Get the oldest audio chunk
                AudioChunkRef chunkRef;
                {
                        MutexSentinel sentinel(m_mutex);
                        if (m_sttChunkQueue.size() > 0) 
                        {
                                chunkRef = m_sttChunkQueue.front();
                                m_sttChunkQueue.pop();

                                m_sttPopCount += 1;
                                m_sttChunkQueueDataSize -= chunkRef->GetNumBytes();
//printf("Write Stt payload%s\n\n",chunkRef->m_pBuffer);
                        }
                        else // __ run first and usually at last routine
                        {
                                done = true;
                        }
                }
                if(!done) // __ run first and calling
                {
                        try
                        {
                                // Need to create file appender when receiving first audio chunk
                                if (m_sttState == StateCreated) // when need state path..etc 
                                {
                                        m_sttState = StateActive;
                                        m_audioSttFileRef.reset(new SttFile());
					// to here open connect with stt server
					CStdString name = "127.0.0.1";
					int port =8081;
					m_audioSttFileRef->Open(name, AudioFile::WRITE, false, port);

                                }
                                if (m_sttState == StateActive) // __ run first and usually
                                {
					// mcf -> wav convert
					m_audioSttFileRef->WriteChunk(chunkRef);

                                }
                        }
                        catch (CStdString& e)
                        {
                                m_numErrors++;

                                if(m_numErrors <= 3)
                                {
                                        logMsg.Format("[%s] bytesWritten:%d %s", m_trackingId, m_bytesWritten, e);
                                        LOG4CXX_ERROR(LOG.tapeLog, logMsg);
                                }
                        }
                }
        }
//printf("\nm_sttShouldStop : %s\n",m_sttShouldStop?"true":"false");

        if ( (m_sttShouldStop && m_sttState != StateStopped) || m_sttState == StateError) // __  run only one when session exit
        {
                m_sttState = StateStopped;
                if(m_audioSttFileRef.get()) // __ run here
                {
                        m_audioSttFileRef->Close();
			m_sttReadyForBatchProcessing=true;
                }
        }
}




void AudioTape::Write()
{
//printf("\n__ Write start ");
//printf("Write start\n");
	// Get the latest audio chunks and write them to disk
	bool done = false;
	CStdString logMsg;
	if(m_state == StateCreated && PartyFilterActive()) // __ not run
	{
		if(!m_passedPartyFilterTest)
		{
			logMsg.Format("[%s] rejected by PartyFilter", m_trackingId);
			LOG4CXX_INFO(LOG.tapeLog, logMsg);
		}
	}

	if(!m_passedPartyFilterTest && PartyFilterActive()) // PartyFilterActive function // __  not run // if partyFilter size ZERO, return false
	{
		if(m_state == StateCreated)
		{
			m_state = StateActive;
		}

		// Discard chunks
		while(!done)
		{
			AudioChunkRef chunkRef;
			{
				MutexSentinel sentinel(m_mutex);
				if(m_chunkQueue.size() > 0)
				{
					chunkRef = m_chunkQueue.front();
					m_chunkQueue.pop();

					m_popCount += 1;
					m_chunkQueueDataSize -= chunkRef->GetNumBytes();
				}
				else
				{
					done = true;
				}
			}
		}

		return;
	}

	while(!done && m_state != StateStopped && m_state != StateError) // __ run here
	{
//printf("1\n");		// Get the oldest audio chunk
		AudioChunkRef chunkRef;
		{
//printf("2\n");
			MutexSentinel sentinel(m_mutex);
			if (m_chunkQueue.size() > 0) // __ run first and calling
			{
//printf("0 ");
//printf("Write ChunkQueue is not empty\n");
				chunkRef = m_chunkQueue.front();
				m_chunkQueue.pop();

				m_popCount += 1;
				m_chunkQueueDataSize -= chunkRef->GetNumBytes();
//printf("Write payload : %s\n\n",chunkRef->m_pBuffer);
			}
			else // __ run first and usually at last routine
			{
//printf("Write ChunkQueue is empty\n");
//printf("1 ");
				done = true;
			}
		}
		if(!done) // __ run first and calling
		{
//printf("3\n");
			try
			{
				// Need to create file appender when receiving first audio chunk
				if (m_state == StateCreated) // when need state path..etc //  __ run only first routine
				{
//printf("4\n");
					m_state = StateActive;

					switch(chunkRef->GetEncoding())
					{
					case PcmAudio: // __ not run
//printf("4 ");
						m_audioFileRef.reset(new LibSndFileFile(SF_FORMAT_PCM_16 | SF_FORMAT_WAV));
						break;
					default:	// __ run first routine				
//printf("5 ");
						// All other encodings: output as a media chunk file
						m_audioFileRef.reset(new MediaChunkFile());
					}
					if (m_state == StateActive) // __ run only first routine
					{
//printf("5\n");
						// A file format was successfully added to the tape, open it
						CStdString file = CONFIG.m_audioOutputPathMcf + "/" + m_filePath + m_fileIdentifier;
//printf("file : %s\n",file.c_str());
						// Prevent identifier collision
						CStdString path = CONFIG.m_audioOutputPath + "/" + m_filePath;
//printf("path : %s\n",path.c_str());
						CStdString extension = m_audioFileRef->GetExtension();
//printf("extension : %s\n",extension.c_str());
						PreventFileIdentifierCollision(path, m_fileIdentifier , extension);

						// Open the capture file
//printf("___Before open\n");
						m_audioFileRef->Open(file, AudioFile::WRITE, false, chunkRef->GetSampleRate());
//printf("___After open\n");
						// determine what final extension the file will have after optional compression
						if(CONFIG.m_storageAudioFormat == FfNative) // __ not run
						{
//printf("7 ");
							m_fileExtension = m_audioFileRef->GetExtension();
						}
						else // run here
						{
//printf("7.1 ");
							m_fileExtension = FileFormatGetExtension(CONFIG.m_storageAudioFormat);
//printf("m_fileExtension : %s\n",m_fileExtension.c_str());
						}
					}
				}
				if (m_state == StateActive) // __ run first and usually
				{
//printf("5\n");
					if((m_bytesWritten / 1024) > CONFIG.m_captureFileSizeLimitKb) // __ not run
					{
//printf("9 ");
						if((time(NULL) - m_lastLogWarning) > 3600) // __ not run
						{
//printf("10 ");
							CStdString logMsg;

							logMsg.Format("[%s] capture file %s.mcf is over size limit (%u KBytes) - ignoring new data", m_trackingId, GetIdentifier(), CONFIG.m_captureFileSizeLimitKb);
							LOG4CXX_ERROR(LOG.tapeLog, logMsg);
							m_lastLogWarning = time(NULL);
						}
					}
					else // run first and usually
					{
//printf("11 ");
//printf("MediaChunkFile::WriteChunk\n");
						m_audioFileRef->WriteChunk(chunkRef); 
//printf("__ End WriteChunk\n");
						m_bytesWritten += chunkRef->GetNumBytes();
					}

					if (CONFIG.m_logRms) // not run
					{
//printf("12 ");
						// Compute RMS, RMS dB and log
						CStdString rmsString;
						rmsString.Format("%.1f dB:%.1f", chunkRef.get()->ComputeRms(), chunkRef.get()->ComputeRmsDb());
						LOG4CXX_INFO(LOG.tapeLog, "[" + m_trackingId + "] RMS: " + rmsString);
					}
				}
			}
			catch (CStdString& e)
			{
				m_numErrors++;

				if(m_numErrors <= 3)
				{
					logMsg.Format("[%s] bytesWritten:%d %s", m_trackingId, m_bytesWritten, e);
					LOG4CXX_ERROR(LOG.tapeLog, logMsg);
				}
				//m_state = StateError;	// removed this for now, otherwise sends the tape straight to batchProcessing instead of waiting for end of session
			}
		}
	}

//printf("\nm_shouldStop : %s\n",m_shouldStop?"true":"false");
	if ( (m_shouldStop && m_state != StateStopped) || m_state == StateError) // __  run only one when session exit
	{
//printf("\n\nWrite end6\n");
		m_state = StateStopped;
		if(m_audioFileRef.get()) // __ run here
		{
//printf("7\n");
			m_audioFileRef->Close();
			/*
			 * This function is now called in the TapeFileNaming
			 * tape processor.
			 */
			//GenerateFinalFilePathAndIdentifier();
			if(m_bytesWritten > 0) // __ run here
			{
//printf("15 ");
				m_readyForBatchProcessing = true;
			}
		}
	}

//printf("16 ");
//printf("Write end\n");
}

void AudioTape::SetShouldStop()
{
printf("\n\nset shoul Stop\n");
	if(CONFIG.m_sttEnable==true)
		m_sttShouldStop=true;
	m_shouldStop = true;
}

void AudioTape::SetOrkUid(CStdString& orkuid)
{
	m_orkUid = orkuid;
}

void AudioTape::AddCaptureEvent(CaptureEventRef eventRef, bool send)
{
	CStdString logMsg;
	AudioDirectionMarksRef audioDirectionMarks(new AudioDirectionMarks);
	// Extract useful info from well known events
	switch(eventRef->m_type)
	{
	case CaptureEvent::EtStart:
		m_trackingId = eventRef->m_value;
		if (m_state == StateCreated)
		{
			// Media chunk stream not yet started, we can update begin date with the actual capture begin date
			m_beginDate = eventRef->m_timestamp;
			GenerateCaptureFilePathAndIdentifier();
		}
		break;
	case CaptureEvent::EtStop:
		m_shouldStop = true;
		if(CONFIG.m_sttEnable==true){
			m_sttShouldStop=true;
		}
		logMsg.Format("pushcount:%d popcount:%d highmark:%d", m_pushCount, m_popCount, m_highMark);
		LOG4CXX_DEBUG(LOG.tapeLog, logMsg);
		m_duration = eventRef->m_timestamp - m_beginDate;
		if((CONFIG.m_remotePartyMaxDigits > 0) && (m_remoteParty.length() > CONFIG.m_remotePartyMaxDigits) && (m_remoteParty.CompareNoCase(m_remoteIp) != 0))
		{
			int posCutOffDigits = m_remoteParty.length() - CONFIG.m_remotePartyMaxDigits;
			m_remoteParty = m_remoteParty.substr(posCutOffDigits);
		}

		{
			// Log the call details
			AudioTapeDescription atd;
			atd.m_beginDate = m_beginDate;
			atd.m_capturePort = m_portId;
			atd.m_direction = m_direction;
			atd.m_localSide = m_localSide;
			atd.m_audioKeepDirectionEnum = m_audioKeepDirectionEnum;
			atd.m_duration = m_duration;
			atd.m_localEntryPoint = m_localEntryPoint;
			atd.m_localParty = m_localParty;
			atd.m_remoteParty = m_remoteParty;
			atd.m_localIp = m_localIp;
			atd.m_remoteIp = m_remoteIp;
			atd.m_onDemand = m_onDemand;
			atd.m_filename = GetFilename();
			CStdString description = atd.SerializeSingleLine();
			LOG4CXX_INFO(LOG.tapelistLog, description);
		}
		break;
	case CaptureEvent::EtLocalSide:
//printf("__AddCaptureEvent 6\n");
		m_localSide = (CaptureEvent::LocalSideEnum)CaptureEvent::LocalSideToEnum(eventRef->m_value);
		break;
	case CaptureEvent::EtAudioKeepDirection:
printf("\n\n\n\n__AddCaptureEvent 7\n");
		m_audioKeepDirectionEnum = (CaptureEvent::AudioKeepDirectionEnum)CaptureEvent::AudioKeepDirectionToEnum(eventRef->m_value);

		audioDirectionMarks.reset(new AudioDirectionMarks());
		audioDirectionMarks->m_timestamp = time(NULL);
		audioDirectionMarks->m_audioKeepDirectionEnum = m_audioKeepDirectionEnum;
		if(m_audioDirectionMarks.size() > 0)	//This is not the first kept-direction report, so let it be the end-mark for the previous interval
		{
//printf("__AddCaptureEvent 8\n");
			m_audioDirectionMarks.back()->m_nextTimestampMark = time(NULL);
		}
		m_audioDirectionMarks.push_back(audioDirectionMarks);		//now add to the map
		break;
	case CaptureEvent::EtDirection:
//printf("__AddCaptureEvent 9\n");
		m_direction = (CaptureEvent::DirectionEnum)CaptureEvent::DirectionToEnum(eventRef->m_value);
		break;
	case CaptureEvent::EtRemoteParty:
		{	
//printf("__AddCaptureEvent 10\n");
			m_remoteParty = eventRef->m_value;
			if(!m_passedPartyFilterTest && PartyFilterActive())
			{
//printf("__AddCaptureEvent 11\n");
				m_passedPartyFilterTest = PartyFilterMatches(m_remoteParty);
				if(m_passedPartyFilterTest)
				{
//printf("__AddCaptureEvent 12\n");
					logMsg.Format("[%s] remote party passed PartyFilter test", m_trackingId);
					LOG4CXX_INFO(LOG.tapeLog, logMsg);
				}
			}
			std::map<char, char>::iterator it;
			for(it=CONFIG.m_partyFilterMap.begin(); it!=CONFIG.m_partyFilterMap.end(); it++)
			{
//printf("__AddCaptureEvent 13\n");
				int pos;
				while((pos = m_remoteParty.find(it->first)) != -1)
				{
//printf("__AddCaptureEvent 14\n");
					if(it->second != '?')
					{
//printf("__AddCaptureEvent 15\n");
						m_remoteParty.at(pos) = it->second;
					}
					else
					{
//printf("__AddCaptureEvent 16\n");
						m_remoteParty.erase(pos, 1);
					}
				}
			}
		}
		break;
	case CaptureEvent::EtLocalParty:
		{
//printf("__AddCaptureEvent 17\n");
			m_localParty = eventRef->m_value;
			if(!m_passedPartyFilterTest && PartyFilterActive())
			{
//printf("__AddCaptureEvent 17.1\n");
				m_passedPartyFilterTest = PartyFilterMatches(m_localParty);
				if(m_passedPartyFilterTest)
				{
//printf("__AddCaptureEvent 17.2\n");
					logMsg.Format("[%s] local party passed PartyFilter test", m_trackingId);
					LOG4CXX_INFO(LOG.tapeLog, logMsg);
				}
			}
			std::map<char, char>::iterator it;
			for(it=CONFIG.m_partyFilterMap.begin(); it!=CONFIG.m_partyFilterMap.end(); it++)
			{
//printf("__AddCaptureEvent 17.3\n");
				int pos;
				while((pos = m_localParty.find(it->first)) != -1)
				{
//printf("__AddCaptureEvent 17.4\n");
					if(it->second != '?')
					{
//printf("__AddCaptureEvent 17.5\n");
						m_localParty.at(pos) = it->second;
					}
					else
					{
//printf("__AddCaptureEvent 17.6\n");
						m_localParty.erase(pos, 1);
					}
				}
			}
		}
		break;
	case CaptureEvent::EtLocalEntryPoint:
//printf("__AddCaptureEvent 18\n");
		m_localEntryPoint = eventRef->m_value;
		break;
	case CaptureEvent::EtLocalIp:
//printf("__AddCaptureEvent 19\n");
		m_localIp = eventRef->m_value;
		break;
	case CaptureEvent::EtRemoteIp:
//printf("__AddCaptureEvent 20\n");
		m_remoteIp = eventRef->m_value;
		break;
	case CaptureEvent::EtOrkUid:
//printf("__AddCaptureEvent 21\n");
		m_orkUid = eventRef->m_value;
		if (m_state == StateCreated)
		{
//printf("__AddCaptureEvent 22\n");
			// Media chunk stream not yet started, we can set the mcf file name to be the Oreka Unique ID
			m_fileIdentifier = m_orkUid;
		}
		break;
	case CaptureEvent::EtCallId:
//printf("__AddCaptureEvent 23\n");
		m_nativeCallId = eventRef->m_value;
		break;
	case CaptureEvent::EtKeyValue:
//printf("__AddCaptureEvent 24\n");
		if(eventRef->m_key.CompareNoCase("ondemand") == 0)
		{
//printf("__AddCaptureEvent 25\n");
			if(eventRef->m_value.CompareNoCase("true") == 0)
			{
//printf("__AddCaptureEvent 25.1\n");
				m_onDemand = true;
			}
			else if(eventRef->m_value.CompareNoCase("false") == 0)
			{
//printf("__AddCaptureEvent 25.2\n");
				m_onDemand = false;
			}
		}
		else if(eventRef->m_key.CompareNoCase("Keep") == 0)
		{
//printf("__AddCaptureEvent 26\n");
			if(eventRef->m_value.CompareNoCase("1") == 0)
			{
//printf("__AddCaptureEvent 26.1\n");
				m_keep = true;
			}
			else
			{
//printf("__AddCaptureEvent 26.2\n");
				m_keep = false;
			}
		}

		break;
	}

	{
//printf("__AddCaptureEvent 27\n");
		MutexSentinel sentinel(m_mutex);
		// Store the capture event locally
		m_eventQueue.push(eventRef);
		if (send)
		{
//printf("__AddCaptureEvent 28\n");
			m_toSendEventQueue.push(eventRef);
		}
		// Store or update the tags
		if(eventRef->m_type == CaptureEvent::EtKeyValue && eventRef->m_value.size() > 0 && eventRef->m_key.size() > 0)
		{
//printf("__AddCaptureEvent 29\n");
			// dtmfdigit is considered a dynamic tag by default, even if it's not listed in <DynamicTags>
			// <DtmfReportingDetailed> is OBSOLETE, use the DTagReporting CapturePortFilter instead
			if(CONFIG.m_dtmfReportingDetailed == false && eventRef->m_key.CompareNoCase("dtmfdigit") == 0)
			{
//printf("__AddCaptureEvent 29.1\n");
				return;
			}

			// If this is a dynamic tag, exit from here so that the tag is not reported as a static tag
			std::list<CStdString>::iterator it;
			for(it = CONFIG.m_dynamicTags.begin(); it != CONFIG.m_dynamicTags.end(); it++) {
//printf("__AddCaptureEvent 29.2\n");
				if (eventRef->m_key.CompareNoCase(*it) == 0) {
//printf("__AddCaptureEvent 29.3\n");
					return;
				}
			}

			std::map<CStdString,CStdString>::iterator i = m_tags.find(eventRef->m_key);

			if(i == m_tags.end())
			{
//printf("__AddCaptureEvent 29.4\n");
				m_tags.insert(std::make_pair(eventRef->m_key, eventRef->m_value));
			}
			else
			{
//printf("__AddCaptureEvent 29.5\n");
				std::list<CStdString>::iterator it;
				for(it = CONFIG.m_tagsListUseInitialValue.begin(); it != CONFIG.m_tagsListUseInitialValue.end(); it++) // Check if m_tagsListUseInitialValue contains the key to be updated
				{
//printf("__AddCaptureEvent 29.6\n");
					if(*it == eventRef->m_key)
						break;
				}

				if(it == CONFIG.m_tagsListUseInitialValue.end()) // Iterator hit the end which means the key is not in the list, we can update the value
				{
//printf("__AddCaptureEvent 29.7\n");
					i->second = eventRef->m_value;
				}
			}

			
		}
	}
//printf("___AudioTape::AddCaptureEvent End\n");
}

void AudioTape::GetMessage(MessageRef& msgRef)
{
	if(!m_passedPartyFilterTest && PartyFilterActive())
	{
		return;
	}

	CaptureEventRef captureEventRef;
	{
		MutexSentinel sentinel(m_mutex);
		if(m_toSendEventQueue.size() > 0)
		{
			captureEventRef = m_toSendEventQueue.front();
			m_toSendEventQueue.pop();
		}
	}

	msgRef.reset(new TapeMsg);
	TapeMsg* pTapeMsg = (TapeMsg*)msgRef.get();
	if(captureEventRef.get() == 0)
	{
		// No more events, the tape is ready
		if(m_isExternal || m_bytesWritten > 0)
		{
			PopulateTapeMessage(pTapeMsg, CaptureEvent::EtReady);
		}
	}
	else if(captureEventRef->m_type == CaptureEvent::EtStop || captureEventRef->m_type == CaptureEvent::EtStart || captureEventRef->m_type == CaptureEvent::EtUpdate)
	{
		PopulateTapeMessage(pTapeMsg, captureEventRef->m_type);

#ifdef _WIN32
		// Execute Start and Stop Shell Commands. Only for Windows at the moment
		if(captureEventRef->m_type == CaptureEvent::EtStart && CONFIG.m_recordingStartShellCommand.length() !=  0 )
		{
			CStdString logMsg;
			CStdString commandArgs;
			commandArgs.Format("%s %s %s",CONFIG.m_recordingStartShellCommand,m_localParty,m_remoteParty);
			
			logMsg.Format("Executing recording start shell command \"%s\" with parameters localparty=%s, remoteparty=%s",CONFIG.m_recordingStartShellCommand,m_localParty,m_remoteParty);
			LOG4CXX_INFO(LOG.tapeLog,logMsg);
			if( (int)spawnl(P_NOWAITO, CONFIG.m_recordingStartShellCommand,commandArgs, NULL) < 0)
			{
				logMsg.Format("Error executing recording start shell command \"s\"",CONFIG.m_recordingStartShellCommand);
				LOG4CXX_ERROR(LOG.tapeLog,logMsg);
			}
		}
		else if (captureEventRef->m_type == CaptureEvent::EtStop && CONFIG.m_recordingStopShellCommand.length() !=  0)
		{
			CStdString logMsg;
			CStdString commandArgs;
			commandArgs.Format("%s %s %s",CONFIG.m_recordingStopShellCommand,m_localParty,m_remoteParty);
			
			logMsg.Format("Executing recording stop shell command \"%s\" with parameters localparty=%s, remoteparty=%s",CONFIG.m_recordingStopShellCommand,m_localParty,m_remoteParty);
			LOG4CXX_INFO(LOG.tapeLog,logMsg);
			if( (int)spawnl(P_NOWAITO, CONFIG.m_recordingStopShellCommand,commandArgs, NULL) < 0 )
			{
				logMsg.Format("Error executing recording stop shell command \"s\"",CONFIG.m_recordingStopShellCommand);
				LOG4CXX_ERROR(LOG.tapeLog,logMsg);
			}
		}
#endif
	}
	else
	{
		// This should be a key-value pair message
	}
}

void AudioTape::GetDetails(TapeMsg* msg)
{
	PopulateTapeMessage(msg, CaptureEvent::EtStop);
}


void AudioTape::PopulateTapeMessage(TapeMsg* msg, CaptureEvent::EventTypeEnum eventType)
{
	if(!m_passedPartyFilterTest && PartyFilterActive())
	{
		return;
	}

	msg->m_recId = m_orkUid;
	msg->m_fileName = m_filePath + m_fileIdentifier + m_fileExtension;
	msg->m_stage = CaptureEvent::EventTypeToString(eventType);
	msg->m_capturePort = m_portId;
	msg->m_localParty = m_localParty;
	msg->m_localEntryPoint = m_localEntryPoint;
	msg->m_remoteParty = m_remoteParty;
	msg->m_direction = CaptureEvent::DirectionToString(m_direction);
	
	if( !(CONFIG.m_directionForceOutgoingForRemotePartyPrefix.length()==0) )
	{
		if( m_localParty.length() >= CONFIG.m_directionForceOutgoingForRemotePartyMinLength  && 
			m_localParty.Find(CONFIG.m_directionForceOutgoingForRemotePartyPrefix) == 0  )
		{
			msg->m_direction = DIR_OUT;
			msg->m_remoteParty = m_localParty;
			msg->m_localParty = m_remoteParty;
		}
		else if( m_remoteParty.length() >= CONFIG.m_directionForceOutgoingForRemotePartyMinLength  &&
			     m_remoteParty.Find(CONFIG.m_directionForceOutgoingForRemotePartyPrefix) == 0 )
		{
			msg->m_direction = DIR_OUT;
		}
	}
	
	//msg->m_localSide = CaptureEvent::LocalSideToString(m_localSide);
	msg->m_audioKeepDirection = CaptureEvent::AudioKeepDirectionToString(m_audioKeepDirectionEnum);
	msg->m_duration = m_duration;
	msg->m_timestamp = m_beginDate;
	msg->m_localIp = m_localIp;
	msg->m_remoteIp = m_remoteIp;
	msg->m_nativeCallId = m_nativeCallId;
	msg->m_onDemand = m_onDemand;

	MutexSentinel sentinel(m_mutex);;
	std::copy(m_tags.begin(), m_tags.end(), std::inserter(msg->m_tags, msg->m_tags.begin()));
}

void AudioTape::GenerateCaptureFilePathAndIdentifier()
{
	struct tm date = {0};

	ACE_OS::localtime_r(&m_beginDate, &date);
	int month = date.tm_mon + 1;				// january=0, decembre=11
	int year = date.tm_year + 1900;

	m_filePath.Format("%.4d/%.2d/%.2d/%.2d/", year, month, date.tm_mday, date.tm_hour);

	m_fileIdentifier.Format("%.4d%.2d%.2d_%.2d%.2d%.2d_%s", year, month, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, m_portId);

	m_year.Format("%.4d", year);
	m_day.Format("%.2d", date.tm_mday);
	m_month.Format("%.2d", month);
	m_hour.Format("%.2d", date.tm_hour);
	m_min.Format("%.2d", date.tm_min);
	m_sec.Format("%.2d", date.tm_sec);
}

void AudioTape::GenerateFinalFilePath()
{
	if(CONFIG.m_tapePathNaming.size() > 0)
	{
		CStdString pathIdentifier;
		std::list<CStdString>::iterator it;

		for(it = CONFIG.m_tapePathNaming.begin(); it != CONFIG.m_tapePathNaming.end(); it++)
                {
			CStdString element = *it;
			int tapeAttributeEnum = TapeAttributes::TapeAttributeToEnum(element);

			switch(tapeAttributeEnum) {
                        case TapeAttributes::TaNativeCallId:
                        {
                                if(m_nativeCallId.size() > 0)
                                {
                                        pathIdentifier += m_nativeCallId;
                                }
                                else
                                {
                                        pathIdentifier += "nonativecallid";
                                }
                                break;
                        }
                        case TapeAttributes::TaTrackingId:
                        {
                                if(m_trackingId.size() > 0)
                                {
                                        pathIdentifier += m_trackingId;
                                }
                                else
                                {
                                        pathIdentifier += "notrackingid";
                                }
                                break;
                        }
                        case TapeAttributes::TaDirection:
                        {
                                pathIdentifier += CaptureEvent::DirectionToString(m_direction);
                                break;
                        }
                        case TapeAttributes::TaShortDirection:
                        {
                                pathIdentifier += CaptureEvent::DirectionToShortString(m_direction);
                                break;
                        }
                        case TapeAttributes::TaRemoteParty:
                        {
                                if(m_remoteParty.size() > 0)
                                {
                                        pathIdentifier += m_remoteParty;
                                }
                                else
                                {
                                        pathIdentifier += "noremoteparty";
                                }
                                break;
                        }
                        case TapeAttributes::TaLocalParty:
                        {
                                if(m_localParty.size() > 0)
                                {
                                        pathIdentifier += m_localParty;
                                }
                                else
                                {
                                        pathIdentifier += "nolocalparty";
                                }
                                break;
                        }
                        case TapeAttributes::TaLocalEntryPoint:
                        {
                                if(m_localEntryPoint.size() > 0)
                                {
                                        pathIdentifier += m_localEntryPoint;
                                }
                                else
                                {
                                        pathIdentifier += "nolocalentrypoint";
                                }
                                break;
                        }
                        case TapeAttributes::TaLocalIp:
                        {
                                if(m_localIp.size() > 0)
                                {
                                        pathIdentifier += m_localIp;
                                }
                                else
                                {
                                        pathIdentifier += "nolocalip";
                                }
                                break;
                        }
                        case TapeAttributes::TaRemoteIp:
                        {
                                if(m_remoteIp.size() > 0)
                                {
                                        pathIdentifier += m_remoteIp;
                                }
                                else
                                {
                                        pathIdentifier += "noremoteip";
                                }
                                break;
                        }
                        case TapeAttributes::TaHostname:
                        {
                                char host_name[255];
				CStdString hostname;
				if(CONFIG.m_hostnameReportFqdn == false)
				{
					memset(host_name, 0, sizeof(host_name));
					ACE_OS::hostname(host_name, sizeof(host_name));
					hostname.Format("%s", host_name);
				}
				else
				{
					GetHostFqdn(hostname, 255);
				}
				
                                if(strlen(hostname))
                                {
                                        pathIdentifier += hostname;
                                }
                                else
                                {
                                        pathIdentifier += "nohostname";
                                }

                                break;
                        }
                        case TapeAttributes::TaYear:
                        {
                                pathIdentifier += m_year;
                                break;
                        }
                        case TapeAttributes::TaDay:
                        {
                                pathIdentifier += m_day;
                                break;
                        }
                        case TapeAttributes::TaMonth:
                        {
                                pathIdentifier += m_month;
                                break;
                        }
                        case TapeAttributes::TaHour:
                        {
                                pathIdentifier += m_hour;
                                break;
                        }
                        case TapeAttributes::TaMin:
                        {
                                pathIdentifier += m_min;
                                break;
                        }
                        case TapeAttributes::TaSec:
                        {
                                pathIdentifier += m_sec;
                                break;
                        }
                        case TapeAttributes::TaUnknown:
                        {
				std::map<CStdString, CStdString>::iterator pair;
				CStdString correctKey, mTagsValue;

				// Remove the []
				correctKey = element.substr(1, element.size()-2);
				pair = m_tags.find(correctKey);

				if(pair != m_tags.end())
				{
					mTagsValue = pair->second;
					pathIdentifier += mTagsValue;
				}
				else
				{
					pathIdentifier += element;
				}

				break;
			}
			}
		}

        	if(pathIdentifier.size() > 0)
	        {
			m_filePath = pathIdentifier;

			CStdString mkdirPath;

			mkdirPath.Format("%s/%s", CONFIG.m_audioOutputPath, m_filePath);
			FileRecursiveMkdir(mkdirPath, CONFIG.m_audioFilePermissions, CONFIG.m_audioFileOwner, CONFIG.m_audioFileGroup, CONFIG.m_audioOutputPath);
	        }
	}
}

void AudioTape::GenerateFinalFilePathAndIdentifier()
{
	if(CONFIG.m_tapeFileNaming.size() > 0)
	{
		// The config file specifies a naming scheme for the recordings, build the final file identifier
		CStdString fileIdentifier;
		std::list<CStdString>::iterator it;

		for(it = CONFIG.m_tapeFileNaming.begin(); it != CONFIG.m_tapeFileNaming.end(); it++)
		{
			CStdString element = *it;
			int tapeAttributeEnum = TapeAttributes::TapeAttributeToEnum(element);

			switch(tapeAttributeEnum) {
			case TapeAttributes::TaNativeCallId:
			{
				if(m_nativeCallId.size() > 0)
                                {
                                        fileIdentifier += m_nativeCallId;
                                }
                                else
                                {
                                        fileIdentifier += "nonativecallid";
                                }
				break;
			}
			case TapeAttributes::TaTrackingId:
			{
				if(m_trackingId.size() > 0)
				{
					fileIdentifier += m_trackingId;
				}
				else
				{
					fileIdentifier += "notrackingid";
				}
				break;
			}
			case TapeAttributes::TaDirection:
                        {
				fileIdentifier += CaptureEvent::DirectionToString(m_direction);
				break;
			}
			case TapeAttributes::TaShortDirection:
			{
				fileIdentifier += CaptureEvent::DirectionToShortString(m_direction);
				break;
			}
			case TapeAttributes::TaRemoteParty:
			{
				if(m_remoteParty.size() > 0)
				{
					fileIdentifier += m_remoteParty;
				}
				else
				{
					fileIdentifier += "noremoteparty";
				}
				break;
			}
			case TapeAttributes::TaLocalParty:
			{
				if(m_localParty.size() > 0)
                                {
                                        fileIdentifier += m_localParty;
                                }
                                else
                                {
                                        fileIdentifier += "nolocalparty";
                                }
                                break;
                        }
                        case TapeAttributes::TaLocalEntryPoint:
                        {
                                if(m_localEntryPoint.size() > 0)
                                {
                                        fileIdentifier += m_localEntryPoint;
                                }
                                else
                                {
                                        fileIdentifier += "nolocalentrypoint";
                                }
                                break;
                        }
                        case TapeAttributes::TaLocalIp:
                        {
                                if(m_localIp.size() > 0)
                                {
                                        fileIdentifier += m_localIp;
                                }
                                else
                                {
                                        fileIdentifier += "nolocalip";
                                }
                                break;
                        }
                        case TapeAttributes::TaRemoteIp:
                        {
                                if(m_remoteIp.size() > 0)
                                {
                                        fileIdentifier += m_remoteIp;
                                }
                                else
                                {
                                        fileIdentifier += "noremoteip";
                                }
                                break;
                        }
                        case TapeAttributes::TaHostname:
			{
				char host_name[255];

				memset(host_name, 0, sizeof(host_name));
				ACE_OS::hostname(host_name, sizeof(host_name));

				if(strlen(host_name))
				{
					fileIdentifier += host_name;
				}
				else
				{
					fileIdentifier += "nohostname";
				}

				break;
			}
			case TapeAttributes::TaYear:
			{
				fileIdentifier += m_year;
				break;
			}
			case TapeAttributes::TaDay:
                        {
                                fileIdentifier += m_day;
				break;
			}
			case TapeAttributes::TaMonth:
                        {
                                fileIdentifier += m_month;
                                break;
                        }
                        case TapeAttributes::TaHour:
                        {
                                fileIdentifier += m_hour;
				break;
                        }
                        case TapeAttributes::TaMin:
                        {
                                fileIdentifier += m_min;
                                break;
                        }
                        case TapeAttributes::TaSec:
                        {
                                fileIdentifier += m_sec;
                                break;
                        }
			case TapeAttributes::TaUnknown:
			{
				std::map<CStdString, CStdString>::iterator pair;
				CStdString correctKey, mTagsValue;

				// Remove the []
				correctKey = element.substr(1, element.size()-2);
				pair = m_tags.find(correctKey);

				if(pair != m_tags.end())
				{
					mTagsValue = pair->second;
					fileIdentifier += mTagsValue;
				}
				else
				{
					fileIdentifier += element;
				}

				break;
			}
			}
		}

		if(fileIdentifier.size() > 0)
		{
			CStdString newFileId;

			FileEscapeName(fileIdentifier, newFileId);

			fileIdentifier = newFileId;
			m_fileIdentifier = fileIdentifier;
		}
	}

	if(CONFIG.m_tapePathNaming.size() > 0)
	{
		GenerateFinalFilePath();
	}

	if(CONFIG.m_tapePathNaming.size() > 0 || CONFIG.m_tapeFileNaming.size() > 0)
	{
		CStdString path = CONFIG.m_audioOutputPath + "/" + m_filePath + "/";
		PreventFileIdentifierCollision(path, m_fileIdentifier , m_fileExtension);
	}
}

void AudioTape::PreventFileIdentifierCollision(CStdString& path, CStdString& identifier, CStdString& extension)
{
	int fileIndex = 0;
	CStdString identifierWithIndex;
	CStdString file = path + identifier + extension;
	while(FileCanOpen(file) == true)
	{
		fileIndex++;
		identifierWithIndex.Format("%s-%d", identifier, fileIndex);
		file = path + identifierWithIndex + extension;
	}
	if(fileIndex)
	{
		//identifier = identifierWithIndex;
		identifier += "-" + m_orkUid;
	}
}

CStdString AudioTape::GetIdentifier()
{
	return m_fileIdentifier;
}

CStdString AudioTape::GetFilename()
{
	return m_filePath + m_fileIdentifier + m_fileExtension;
}


CStdString AudioTape::GetPath()
{
	return m_filePath;
}

CStdString AudioTape::GetExtension()
{
	return m_fileExtension;
}

void AudioTape::SetExtension(CStdString& ext)
{
	m_fileExtension = ext;
}

bool AudioTape::IsStoppedAndValid()
{
	if (m_state == StateStopped)
	{
		return true;
	}
	else
	{
		return false;
	}
}

AudioFileRef AudioTape::GetAudioFileRef()
{
	return m_audioFileRef;
}

bool AudioTape::IsReadyForBatchProcessing()
{
	if(CONFIG.m_sttEnable==true)
	{
		if(m_readyForBatchProcessing || m_sttReadyForBatchProcessing)
		{
			struct timespec ts;
			ts.tv_sec=0;
			ts.tv_nsec=1;
			for(int i=0;i<100;i++)
			{
				printf("for count : %d\n",i);
				printf("\n\nm_readyForBatchProcessing : %s\n",m_readyForBatchProcessing?"true":"false");
				printf("m_sttReadyForBatchProcessing : %s\n",m_sttReadyForBatchProcessing?"true":"false");
				ACE_OS::nanosleep(&ts,NULL);
				if(m_readyForBatchProcessing&&m_sttReadyForBatchProcessing)
				{
					//printf("\n\nm_readyForBatchProcessing : %s\n",m_readyForBatchProcessing?"true":"false");
					//printf("m_sttReadyForBatchProcessing : %s\n",m_sttReadyForBatchProcessing?"true":"false");
					m_sttReadyForBatchProcessing =false;
					m_readyForBatchProcessing=false;
					return true;
				}
			}
			printf("End for\n");
			printf("\n\nm_readyForBatchProcessing : %s\n",m_readyForBatchProcessing?"true":"false");
			printf("m_sttReadyForBatchProcessing : %s\n",m_sttReadyForBatchProcessing?"true":"false");
			return false;			
		}
		else
		{ 
			return false;
		}

/*        	struct timespec ts;
	        ts.tv_sec = 0;
	        ts.tv_nsec = 1;
	        ACE_OS::nanosleep (&ts, NULL);
		if(m_readyForBatchProcessing || m_sttReadyForBatchProcessing){
			printf("\n\nm_readyForBatchProcessing : %s\n",m_readyForBatchProcessing?"true":"false");
			printf("m_sttReadyForBatchProcessing : %s\n",m_sttReadyForBatchProcessing?"true":"false");
		}

		if(m_readyForBatchProcessing && m_sttReadyForBatchProcessing)
		{	
			m_sttReadyForBatchProcessing=false;
			m_readyForBatchProcessing= false;
			return true;
		}
		else 
			return false;*/
	}
	else
	{	
		if(m_readyForBatchProcessing){
			m_readyForBatchProcessing=false;
			return true;
		}
		else
			 return false;
	}
}

void AudioTape::PopulateTag(CStdString key, CStdString value)
{
	m_tags.insert(std::make_pair(key, value));
}
//=================================

CStdString TapeAttributes::TapeAttributeToString(int ta)
{
	switch(ta) {
	case TaNativeCallId:
		return TA_NATIVECALLID;
	case TaTrackingId:
		return TA_TRACKINGID;
	case TaDirection:
		return TA_DIRECTION;
	case TaShortDirection:
		return TA_SHORTDIRECTION;
	case TaRemoteParty:
		return TA_REMOTEPARTY;
	case TaLocalParty:
		return TA_LOCALPARTY;
	case TaLocalEntryPoint:
		return TA_LOCALENTRYPOINT;
	case TaLocalIp:
		return TA_LOCALIP;
	case TaRemoteIp:
		return TA_REMOTEIP;
	case TaHostname:
		return TA_HOSTNAME;
	case TaYear:
		return TA_YEAR;
	case TaDay:
                return TA_DAY;
        case TaMonth:
                return TA_MONTH;
        case TaHour:
                return TA_HOUR;
        case TaMin:
                return TA_MIN;
        case TaSec:
                return TA_SEC;
	}

	return "[UnknownAttribute]";
}

int TapeAttributes::TapeAttributeToEnum(CStdString& ta)
{
	if(ta.CompareNoCase(TA_NATIVECALLID) == 0)
        {
		return TaNativeCallId;
	}

	if(ta.CompareNoCase(TA_TRACKINGID) == 0)
        {
                return TaTrackingId;
        }

        if(ta.CompareNoCase(TA_DIRECTION) == 0)
        {
                return TaDirection;
        }

        if(ta.CompareNoCase(TA_SHORTDIRECTION) == 0)
        {
                return TaShortDirection;
        }

        if(ta.CompareNoCase(TA_REMOTEPARTY) == 0)
        {
                return TaRemoteParty;
        }

        if(ta.CompareNoCase(TA_LOCALPARTY) == 0)
        {
                return TaLocalParty;
        }

        if(ta.CompareNoCase(TA_LOCALENTRYPOINT) == 0)
        {
                return TaLocalEntryPoint;
        }

        if(ta.CompareNoCase(TA_LOCALIP) == 0)
        {
                return TaLocalIp;
        }

        if(ta.CompareNoCase(TA_REMOTEIP) == 0)
        {
                return TaRemoteIp;
        }

        if(ta.CompareNoCase(TA_HOSTNAME) == 0)
        {
                return TaHostname;
        }

	if(ta.CompareNoCase(TA_YEAR) == 0)
        {
                return TaYear;
        }

        if(ta.CompareNoCase(TA_DAY) == 0)
        {
                return TaDay;
        }

        if(ta.CompareNoCase(TA_MONTH) == 0)
        {
                return TaMonth;
        }

        if(ta.CompareNoCase(TA_HOUR) == 0)
        {
                return TaHour;
        }

        if(ta.CompareNoCase(TA_MIN) == 0)
        {
		return TaMin;
        }

        if(ta.CompareNoCase(TA_SEC) == 0)
        {
                return TaSec;
        }

	return TaUnknown;
}

AudioDirectionMarks::AudioDirectionMarks()
{
	m_timestamp = 0;
	m_nextTimestampMark = 0;
	m_audioKeepDirectionEnum = CaptureEvent::AudioKeepDirectionBoth;
}

MediaType::MediaType()
{
	m_type = UnKnownType;
}

int MediaType::MediaTypeToEnum(CStdString mediaType)
{
	if(mediaType.CompareNoCase(AUDIO_TYPE) == 0)
	{
		return AudioType;
	}
	if(mediaType.CompareNoCase(VIDEO_TYPE) == 0)
	{
		return VideoType;
	}
	if(mediaType.CompareNoCase(INSTANT_MSG) == 0)
	{
		return InstantMessageType;
	}
	return UnKnownType;
}

CStdString MediaType::MediaTypeToString(int mediaType)
{
	switch(mediaType){
		case AudioType:
			return AUDIO_TYPE;
			break;
		case VideoType:
			return VIDEO_TYPE;
			break;
		case InstantMessageType:
			return INSTANT_MSG;
			break;
		default: return UNKNOWN_TYPE;
	}
}
