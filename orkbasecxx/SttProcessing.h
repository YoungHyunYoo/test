#ifndef __STTPROCESSING_H__
#define __STTPROCESSING_H__

//#include "ConfigManager.h"
//#include "TapeProcessor.h"
#include "ThreadSafeQueue.h"
#include "AudioTape.h"


//class SttProcessing;
 
//class DLL_IMPORT_EXPORT_ORKBASE SttProcessing : public TapeProcessor
 
class DLL_IMPORT_EXPORT_ORKBASE SttProcessing  
{
public:
//	static void Initialize(); 
//	CStdString __CDECL__ GetName();
//      TapeProcessorRef __CDECL__ Instanciate();
	SttProcessing();
	static SttProcessing*GetInstance();
        static void ThreadHandler(void *args);
	
//      void __CDECL__ AddAudioTape(AudioTapeRef& audioTapeRef);
        void AddAudioTape(AudioTapeRef audioTapeRef);

	AudioTapeRef Pop(CStdString &after);
	AudioTapeRef Pop();
	void Push(AudioTapeRef& audioTapeRef);

private:
	
        static SttProcessing m_sttProcessingSingleton;

	std::map<CStdString,AudioTapeRef> m_sttAudioTapeQueue;
	ACE_Thread_Mutex m_mutex;
	ACE_Thread_Semaphore m_semaphore;

	time_t m_lastQueueFullTime;

//      ThreadSafeQueue<AudioTapeRef> m_audioTapeQueue;

//      size_t m_threadCount;
//      ACE_Thread_Mutex m_mutex;
//      int m_currentDay;

};




#endif
