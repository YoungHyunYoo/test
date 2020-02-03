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
#pragma warning( disable: 4786 ) // disables truncated symbols in browse-info warning

#include "ConfigManager.h"
#include "SttFile.h"
#include "Utils.h"
#include "LogManager.h"
#include <netdb.h>

#define MAX_CHUNK_SIZE 100000


SttFile::SttFile()
{
	m_stream = NULL;
	m_sttStream = NULL;
	m_mode = READ;
	m_numChunksWritten = 0;
	m_sampleRate = 0;

	m_sttChunkQueueDataSize = 0;
}

SttFile::~SttFile()
{
	Close();
}


void SttFile::Close()
{
	if(m_sttStream)
	{
		FlushToDisk();
		close(m_sttStream);
		m_sttStream = NULL;
	}
}

bool SttFile::FlushToDisk()
{
        bool writeError = false;
        while(m_sttChunkQueue.size() > 0) // __ run here
        {
                AudioChunkRef tmpChunk = m_sttChunkQueue.front();
                m_sttChunkQueue.pop();
                if(tmpChunk.get() == NULL)
                {
                        continue;
                }
                if(tmpChunk->m_pBuffer == NULL) // __ not run
                {
                        continue;
                }
                int numWritten = ACE_OS::write(m_sttStream,tmpChunk->GetDetails(), sizeof(AudioChunkDetails));
                if(numWritten ==-1) // __ not run
                {
                        writeError = true;
                        break;
                }
                numWritten = ACE_OS::write(m_sttStream,tmpChunk->m_pBuffer, tmpChunk->GetNumBytes());
                if(numWritten == -1) // __ not run
                {
                        writeError = true;
                        break;
                }
        }
        m_sttChunkQueueDataSize = 0;
        return writeError;
}


void SttFile::WriteChunk(AudioChunkRef chunkRef)
{
printf("1 SttFile::WriteChunk : %d\n",chunkRef.use_count());
        if(chunkRef.get() == NULL) // __ not run
        {
                return;
        }
        if(chunkRef->GetDetails()->m_numBytes == 0)
        {
                return;
        }

        bool writeError = false;

printf("2 SttFile::WriteChunk : %d\n",chunkRef.use_count());
        AudioChunk* pChunk = chunkRef.get();
        m_sttChunkQueueDataSize += pChunk->GetNumBytes();
        m_sttChunkQueue.push(chunkRef);
printf("3 SttFile::WriteChunk : %d\n",chunkRef.use_count());
        if(m_sttChunkQueueDataSize > (unsigned int)(CONFIG.m_captureFileBatchSizeKByte*258)) // __ run last time or Q size is full
        {
printf("4 SttFile::WriteChunk : %d\n",chunkRef.use_count());
                if (m_sttStream) // __ run here
                {
                        writeError = FlushToDisk();
printf("5 SttFile::WriteChunk : %d\n",chunkRef.use_count());
                }
                else // __ not run
                {
                        throw(CStdString("Stt Server disconnected"));
                }
        }
printf("6 SttFile::WriteChunk : %d\n",chunkRef.use_count());
        if (writeError)// __ not run
        {
        throw(CStdString("Could not write to file:")+ m_filename);
        }

}

int SttFile::ReadChunkMono(AudioChunkRef& chunkRef)
{
	return 0;
}


void SttFile::Open(CStdString& filename, fileOpenModeEnum mode, bool stereo, int port)
{
	  CStdString logMsg;
          int clientfd;
          struct hostent *hp;
          struct sockaddr_in serveraddr;
          const char * hostname =filename.c_str();
          if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                return;

          if ((hp = gethostbyname(hostname)) == NULL)
              return;
          bzero((char *) &serveraddr, sizeof(serveraddr));
          serveraddr.sin_family = AF_INET;
          bcopy((char *)hp->h_addr,
                (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
                 serveraddr.sin_port = htons(port);

          if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0){
	 	logMsg.Format("Stt Server Connection fail");
         	LOG4CXX_ERROR(LOG.sttProcessingLog,logMsg);
	        return;
	  }

          m_sttStream= clientfd;
	
	  logMsg.Format("Stt Server Connection success");
	  LOG4CXX_INFO(LOG.sttProcessingLog,logMsg);

}

CStdString SttFile::GetExtension()
{
	return ".mcf";
}

void SttFile::SetNumOutputChannels(int numChan)
{

}

