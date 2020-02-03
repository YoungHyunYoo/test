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

#include "ImmediateProcessing.h"
#include "LogManager.h"
#include "ace/OS_NS_unistd.h"
#include "BatchProcessing.h"
#include "Daemon.h"
#include "ConfigManager.h"
#include "TapeProcessor.h"


ImmediateProcessing ImmediateProcessing::m_immediateProcessingSingleton;

ImmediateProcessing::ImmediateProcessing()
{
	m_lastQueueFullTime = time(NULL);
	m_semaphore.acquire();
}

ImmediateProcessing* ImmediateProcessing::GetInstance()
{
	return &m_immediateProcessingSingleton;
}

void ImmediateProcessing::AddAudioTape(AudioTapeRef audioTapeRef)
{
//printf("___AddAudioTape\n");
	Push(audioTapeRef);
}

AudioTapeRef ImmediateProcessing::Pop(CStdString& after)
{
	m_semaphore.acquire();
	MutexSentinel mutexSentinel(m_mutex);
	std::map<CStdString, AudioTapeRef>::iterator begin;
	std::map<CStdString, AudioTapeRef>::iterator upper;
	AudioTapeRef audioTapeRef;
	begin = m_audioTapeQueue.begin();
	if(begin != m_audioTapeQueue.end())
	{
		if(after.size() == 0)
		{
			audioTapeRef = begin->second;
		}
		else
		{
			upper = m_audioTapeQueue.upper_bound(after);
			if(upper == m_audioTapeQueue.end())
			{
				audioTapeRef = begin->second;
			}
			else
			{
				audioTapeRef = upper->second;
			}
		}

		if(audioTapeRef.get() != NULL)
		{
			m_audioTapeQueue.erase(audioTapeRef->m_portId);
		}
		else
		{
			CStdString key = "NULL";

			m_audioTapeQueue.erase(key);
		}

	}


	return audioTapeRef;
}

void ImmediateProcessing::Push(AudioTapeRef& audioTapeRef)
{
//printf("______Immediate Push\n");
	MutexSentinel mutexSentinel(m_mutex);
	CStdString key;

	if(audioTapeRef.get() == NULL)
	{
		key = "NULL";
	}
	else
	{
		key = audioTapeRef->m_portId;
	}

	m_audioTapeQueue.erase(key);
	m_audioTapeQueue.insert(std::make_pair(key, audioTapeRef));

	m_semaphore.release();
}

void ImmediateProcessing::ThreadHandler(void *args)
{
	SetThreadName("orka:imm");
	CStdString logMsg;
	CStdString lastHandled;

	ImmediateProcessing* pImmediateProcessing = ImmediateProcessing::GetInstance();

	logMsg.Format("thread starting - queue size:%d", CONFIG.m_immediateProcessingQueueSize);
	LOG4CXX_INFO(LOG.immediateProcessingLog, logMsg);

	bool stop = false;

	for(;stop == false;)
	{

		try
		{
//printf("__Immeditate before Pop\n");
//getchar();
			AudioTapeRef audioTapeRef = pImmediateProcessing->Pop(lastHandled);
//printf("Immediate Pop\n");
//printf("__Immeditate after Pop\n");
			if(audioTapeRef.get() == NULL) // __ not run
			{
				lastHandled = "NULL";
				if(Daemon::Singleton()->IsStopping())
				{
					stop = true;
				}
			}
			else   
			{
				//LOG4CXX_INFO(LOG.immediateProcessingLog, "Previous:" + lastHandled + " Current:" + audioTapeRef->m_portId);
				lastHandled = audioTapeRef->m_portId; // Tracking ID
				audioTapeRef->Write(); // Get the latest audio chunks and write them to disk
				if (audioTapeRef->IsReadyForBatchProcessing()) // __ run when ssession close
				{

					if(CONFIG.m_tapeDurationMinimumSec>0 && audioTapeRef->m_duration<CONFIG.m_tapeDurationMinimumSec) // __ not run
					{
						audioTapeRef->GetAudioFileRef()->Delete();

						CStdString logMsg;
						logMsg.Format("[%s] is less than %d sec, discarding", audioTapeRef->m_trackingId, CONFIG.m_tapeDurationMinimumSec);
						LOG4CXX_INFO(LOG.immediateProcessingLog, logMsg);
					}
					else if(audioTapeRef->m_keep == false) // __ not run
					{
						audioTapeRef->GetAudioFileRef()->Delete();
						CStdString logMsg;
						logMsg.Format("[%s] Do Not Keep detected , deleting", audioTapeRef->m_trackingId, CONFIG.m_tapeDurationMinimumSec);
						LOG4CXX_INFO(LOG.immediateProcessingLog, logMsg);
					}
					else // __ run when only session close
					{ 
						// Pass the tape to the tape processor chain
						TapeProcessorRegistry::instance()->RunProcessingChain(audioTapeRef);
					}
				}
			}
		}
		catch (CStdString& e)
		{
			LOG4CXX_ERROR(LOG.immediateProcessingLog, CStdString("ImmediateProcessing: ") + e);
		}
	}
	LOG4CXX_INFO(LOG.immediateProcessingLog, CStdString("Exiting thread"));
}


