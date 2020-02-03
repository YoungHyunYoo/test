#pragma warning( disable: 4786 )

#define _WINSOCKAPI_            // prevents the inclusion of winsock.h

#include "SttProcessing.h"
#include "LogManager.h"
#include "ace/OS_NS_unistd.h"
#include "Daemon.h"
#include "ConfigManager.h"
//#include "TapeProcessor.h"


SttProcessing SttProcessing::m_sttProcessingSingleton;

SttProcessing::SttProcessing()
{
        m_lastQueueFullTime = time(NULL);
        m_semaphore.acquire();
}

SttProcessing* SttProcessing::GetInstance()
{
        return &m_sttProcessingSingleton;
}

void SttProcessing::AddAudioTape(AudioTapeRef audioTapeRef)
{
//printf("___AddAudioTape\n");
        Push(audioTapeRef);
}

AudioTapeRef SttProcessing::Pop(CStdString& after)
{
        m_semaphore.acquire();
        MutexSentinel mutexSentinel(m_mutex);
        std::map<CStdString, AudioTapeRef>::iterator begin;
        std::map<CStdString, AudioTapeRef>::iterator upper;
        AudioTapeRef audioTapeRef;
        begin = m_sttAudioTapeQueue.begin();
        if(begin != m_sttAudioTapeQueue.end())
        {
                if(after.size() == 0)
                {
                        audioTapeRef = begin->second;
                }
                else
                {
                        upper = m_sttAudioTapeQueue.upper_bound(after);
                        if(upper == m_sttAudioTapeQueue.end())
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
                        m_sttAudioTapeQueue.erase(audioTapeRef->m_portId);
                }
                else
                {
                        CStdString key = "NULL";

                        m_sttAudioTapeQueue.erase(key);
                }

        }


        return audioTapeRef;
}

void SttProcessing::Push(AudioTapeRef& audioTapeRef)
{
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

        m_sttAudioTapeQueue.erase(key);
        m_sttAudioTapeQueue.insert(std::make_pair(key, audioTapeRef));

        m_semaphore.release();
}

void SttProcessing::ThreadHandler(void *args)
{
        SetThreadName("orka:stt");
        CStdString logMsg;
        CStdString lastHandled;

        SttProcessing* pSttProcessing = SttProcessing::GetInstance();

        logMsg.Format("Stt thread starting - queue size:");
        LOG4CXX_INFO(LOG.sttProcessingLog, logMsg);

        bool stop = false;

        for(;stop == false;)
        {

                try
                {
                        AudioTapeRef audioTapeRef = pSttProcessing->Pop(lastHandled);
//printf("Stt Pop\n");
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
                                //LOG4CXX_INFO(LOG.sttProcessingLog, "Previous:" + lastHandled + " Current:" + audioTapeRef->m_portId);
                                lastHandled = audioTapeRef->m_portId; // Tracking ID
                                audioTapeRef->WriteStt(); // Get the latest audio chunks and write them to disk
                        }
                }
                catch (CStdString& e)
                {
                        LOG4CXX_ERROR(LOG.sttProcessingLog, CStdString("SttProcessing: ") + e);
                }
        }
        LOG4CXX_INFO(LOG.immediateProcessingLog, CStdString("Exiting thread"));
}




/*#pragma warning ( disable:4786 )

#include "SttProcessing.h"
#include "Daemon.h"
TapeProcessorRef SttProcessing::m_singleton;


void SttProcessing::Initialize()
{
	if(m_singleton.get() == NULL)
        {
		m_singleton.reset(new SttProcessing());
		TapeProcessorRegistry::instance()->RegisterTapeProcessor(m_singleton);
	}
}

SttProcessing::SttProcessing()
{
        m_threadCount = 0;

        struct tm date = {0};
        time_t now = time(NULL);
        ACE_OS::localtime_r(&now, &date);
        m_currentDay = date.tm_mday;
}

CStdString __CDECL__ SttProcessing::GetName()
{
        return "SttProcessing";
}


TapeProcessorRef  SttProcessing::Instanciate()
{
        return m_singleton;
}

void SttProcessing::AddAudioTape(AudioTapeRef& audioTapeRef)
{
        if (!m_audioTapeQueue.push(audioTapeRef))
        {
                // Log error
                LOG4CXX_ERROR(LOG.batchProcessingLog, CStdString("queue full"));
        }
}

void SttProcessing::ThreadHandler(void *args)
{
	printf("\n\n__Start SttProcessing\n");
        SetThreadName("orka:stt");

        CStdString logMsg;

        CStdString processorName("SttProcessing");
        TapeProcessorRef sttProcessing = TapeProcessorRegistry::instance()->GetNewTapeProcessor(processorName);
	if(sttProcessing.get() == NULL)
	{
	       	printf("Could not instanciate BatchProcessing\n");
                return;
	}
	
	SttProcessing* pSttProcessing = (SttProcessing*)(sttProcessing->Instanciate().get());

        bool stop = false;

        for(;stop == false;)
        {
                AudioTapeRef audioTapeRef;
                CStdString trackingId = "[no-trk]";
                try
                {
                        audioTapeRef = pSttProcessing->m_audioTapeQueue.pop();
                        if(audioTapeRef.get() == NULL)
                        {
				printf("audioTapeRef.get() == NULL\n");
                                if(Daemon::Singleton()->IsStopping())
                                {
                                        stop = true;
                                }
                                if(Daemon::Singleton()->GetShortLived())
                                {
                                        Daemon::Singleton()->Stop();
                                }
                        }
                        else
                        {
				printf("\n\nPop from SttProcessing\n");
                                pSttProcessing->RunNextProcessor(audioTapeRef);
                        }
                }
		catch(CStdString&e)
		{
			printf("Catch Error\n");
		}
        }
}*/
