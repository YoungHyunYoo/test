#ifndef __STTPROCESSING_H__
#define __STTPROCESSING_H__

#include "ThreadSafeQueue.h"
#include "AudioTape.h"


 
class DLL_IMPORT_EXPORT_ORKBASE SttProcessing  
{
public:
	SttProcessing();
	static SttProcessing*GetInstance();
        static void ThreadHandler(void *args);
	
        void AddAudioTape(AudioTapeRef audioTapeRef);

	AudioTapeRef Pop(CStdString &after);
	void Push(AudioTapeRef& audioTapeRef);

private:
	
        static SttProcessing m_sttProcessingSingleton;

	std::map<CStdString,AudioTapeRef> m_sttAudioTapeQueue;
	ACE_Thread_Mutex m_mutex;
	ACE_Thread_Semaphore m_semaphore;
	time_t m_lastQueueFullTime;
};




#endif
