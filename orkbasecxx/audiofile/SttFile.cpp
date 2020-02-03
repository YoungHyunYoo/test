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
//	printf("\nIm SttFile created\n\n");
	m_stream = NULL;
	m_sttStream = NULL;
	m_mode = READ;
	m_numChunksWritten = 0;
	m_sampleRate = 0;

	m_chunkQueueDataSize = 0;
}

SttFile::~SttFile()
{
	Close();
}


void SttFile::Close()
{
//	printf("\n\nsttFile::Close()\n");
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
        while(m_chunkQueue.size() > 0) // __ run here

        {
                AudioChunkRef tmpChunk = m_chunkQueue.front();
                m_chunkQueue.pop();
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
                        break;
                }
                numWritten = ACE_OS::write(m_sttStream,tmpChunk->m_pBuffer, tmpChunk->GetNumBytes());
                if(numWritten == -1) // __ not run
                {
                        writeError = true;
                        break;
                }
        }
        m_chunkQueueDataSize = 0;
        return writeError;
}


void SttFile::WriteChunk(AudioChunkRef chunkRef)
{

        if(chunkRef.get() == NULL) // __ not run
        {
                return;
        }
        if(chunkRef->GetDetails()->m_numBytes == 0)
        {
                return;
        }

        bool writeError = false;

        AudioChunk* pChunk = chunkRef.get();
        m_chunkQueueDataSize += pChunk->GetNumBytes();
        m_chunkQueue.push(chunkRef);
        if(m_chunkQueueDataSize > (unsigned int)(CONFIG.m_captureFileBatchSizeKByte*258)) // __ run last time or Q size is full
        {
                if (m_sttStream) // __ run here
                {
                        writeError = FlushToDisk();
                }
                else // __ not run
                {
                        throw(CStdString("Write attempt on unopened file:")+ m_filename);
                }
        }
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
// connect to sttServer
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
		return;
		
	 	logMsg.Format("Stt Server Connection fail");
         	LOG4CXX_ERROR(LOG.sttProcessingLog,logMsg);
	        return;
	  }

          m_sttStream= clientfd;
	
	  logMsg.Format("Stt Server Connection success");
	  LOG4CXX_INFO(LOG.sttProcessingLog,logMsg);

/*	  if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	      return;
	  m_sttStream= clientfd;



  	if(!m_sttStream)
	{
		throw(CStdString("Could not open stream to STT SERVER "));
	}
	else printf("\n\nstream Open!\n");*/

}

CStdString SttFile::GetExtension()
{
	return ".mcf";
}

void SttFile::SetNumOutputChannels(int numChan)
{

}

