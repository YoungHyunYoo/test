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

#define _WINSOCKAPI_		// prevents the inclusion of winsock.h

#include "Utils.h"
#include "LibSndFileFile.h"
#include "ConfigManager.h"

LibSndFileFile::LibSndFileFile(int fileFormat)
{
	m_fileInfo.format = fileFormat;
	m_fileInfo.frames = 0;
	m_fileInfo.samplerate = 0;
	m_fileInfo.channels = 0;
	m_fileInfo.sections = 0;
	m_fileInfo.seekable = 0; 
	m_pFile = NULL;
	m_numChunksWritten = 0;
	m_mode = READ;
	m_sampleRate = 0;
}

LibSndFileFile::~LibSndFileFile()
{
	Close();
}

void LibSndFileFile::Open(CStdString& filename, fileOpenModeEnum mode, bool stereo, int sampleRate)
{
	if(!m_filename.Equals(filename))
	{
		m_filename = filename + ".wav";
	}
	m_mode = mode;
	if(m_numOutputChannels == 2)
	{
		if(CONFIG.m_tapeNumChannels > 1)
		{
			m_fileInfo.channels = CONFIG.m_tapeNumChannels;
		}
		else
		{
			m_fileInfo.channels = 1;
		}
	}
	else
	{
		m_fileInfo.channels = 1;
	}
	if(m_sampleRate == 0)
	{
		m_sampleRate = sampleRate;
		m_fileInfo.samplerate = sampleRate;
	}

	if( (mode==WRITE) && !sf_format_check(&m_fileInfo))
	{
		throw(CStdString("libsndfile: Selected output format not supported"));
	}

	FileRecursiveMkdir(m_filename, CONFIG.m_audioFilePermissions, CONFIG.m_audioFileOwner, CONFIG.m_audioFileGroup, CONFIG.m_audioOutputPath);

	int sndFileMode;
	mode == READ ? sndFileMode = SFM_READ : sndFileMode = SFM_WRITE;
	m_pFile = sf_open((PCSTR)m_filename, sndFileMode, &m_fileInfo);

	if(!m_pFile)
	{
		throw(CStdString("sf_open failed, audio file could not be created:"+ m_filename));
	}
}

void LibSndFileFile::WriteChunk(AudioChunkRef chunkRef)
{
printf("1 LibSndFileFile::WriteChunk : %d\n",chunkRef.use_count());
//printf("___LibSndFileFile::WriteChunk start \n");
//printf("1 ");
	if(chunkRef.get() == NULL) // __ normally the chunks are empty 
	{
//printf("2\n");
		return;
	}
	if(chunkRef->GetDetails()->m_numBytes == 0) // __ not run
	{
//printf("3 ");
		return;
	}
	if (m_pFile) // __ some time chunk is not null // __ some time run here
	{
//printf("4 ");
		if(chunkRef->GetEncoding() == AlawAudio ||  chunkRef->GetEncoding() == UlawAudio) // __ not run
		{
//printf("5 ");
			// We have faith that whoever is producing these chunks created them
			// with the same number of channels that we have opened the soundfile
			// with above - this is enforced in the RtpMixer
			if(chunkRef->m_numChannels > 0 && m_numOutputChannels == 2) // __ not run
			{
//printf("6 ");
				int numBytes = 0;
				unsigned char *muxAudio = NULL;
				unsigned char *wrPtr = NULL;

				numBytes = chunkRef->GetNumSamples() * chunkRef->m_numChannels;
				muxAudio = (unsigned char*)malloc(numBytes);
				wrPtr = muxAudio;

				if(!muxAudio) // __ not run
				{
//printf("7 ");
					CStdString exception;

					exception.Format("sf_write_raw failed for stereo write: could not allocate %d bytes", numBytes);
					throw(exception);
				}

				for(int x = 0; x < chunkRef->GetNumSamples(); x++) // __ not run
				{
//printf("8 ");
					for(int i = 0; i < chunkRef->m_numChannels; i++) // __ not run
					{
//printf("9 ");
						 *wrPtr++ = (unsigned char)*((unsigned char*)(chunkRef->m_pChannelAudio[i])+x);
					}
				}

				if(sf_write_raw(m_pFile, muxAudio, numBytes) != numBytes) // __ not run
				{
//printf("10 ");
					CStdString numChunksWrittenString = IntToString(m_numChunksWritten);
					free(muxAudio);
					throw(CStdString("sf_write_raw failed, audio file " + m_filename + " could not be written after " + numChunksWrittenString + " chunks written"));
				}
				free(muxAudio);
			}
			else // __ not run
			{
//printf("11 ");
				if(sf_write_raw(m_pFile, chunkRef->m_pBuffer, chunkRef->GetNumSamples()) != chunkRef->GetNumSamples()) // __ not run
				{
					CStdString numChunksWrittenString = IntToString(m_numChunksWritten);
					throw(CStdString("sf_write_raw failed, audio file " + m_filename + " could not be written after " + numChunksWrittenString + " chunks written"));
				}
			}
		}
		else if (chunkRef->GetEncoding() == PcmAudio) // __ telephone chunk is PCM // __ some time run here
		{
//printf("12 ");
			// We have faith that whoever is producing these chunks created them
			// with the same number of channels that we have opened the soundfile
			// with above - this is enforced in the RtpMixer
			if(chunkRef->m_numChannels > 0 && m_numOutputChannels == 2) // __ not run
			{
//printf("13 ");
				int numShorts = 0;
				short *muxAudio = NULL;
				short *wrPtr = NULL;

				numShorts = chunkRef->GetNumSamples()*chunkRef->m_numChannels;
				muxAudio = (short*)calloc(numShorts, sizeof(short));
				wrPtr = muxAudio;
				if(!muxAudio) // __ not run
				{
//printf("14 ");
					CStdString exception;

					exception.Format("sf_write_raw failed for stereo write: could not allocate %d bytes", numShorts*sizeof(short));
					throw(exception);
				}
				for(int x = 0; x < chunkRef->GetNumSamples(); x++) // __ not run
				{
//printf("15 ");
					for(int i = 0; i < chunkRef->m_numChannels; i++) // __ not run
					{
//printf("16 ");
						*wrPtr++ = (short)*((short*)(chunkRef->m_pChannelAudio[i])+x);
					}
				}
				if(sf_write_short(m_pFile, muxAudio, numShorts) != numShorts) // __ not run
				{
//printf("17 ");
					CStdString numChunksWrittenString = IntToString(m_numChunksWritten);
					free(muxAudio);
					throw(CStdString("sf_write_short failed, audio file " + m_filename + " could not be written after " + numChunksWrittenString + " chunks written"));
				}
				free(muxAudio);
			}
			else  // __ run here
			{
//printf("18 ");
printf("2 LibSndFileFile::WriteChunk : %d\n",chunkRef.use_count());
				if(sf_write_short(m_pFile, (short*)chunkRef->m_pBuffer, chunkRef->GetNumSamples()) != chunkRef->GetNumSamples()) // __ not run
				{
//printf("19 ");
					CStdString numChunksWrittenString = IntToString(m_numChunksWritten);
					throw(CStdString("sf_write_short failed, audio file " + m_filename + " could not be written after " + numChunksWrittenString + " chunks written"));
				}
			}
		}
		m_numChunksWritten++; // __ just do this AND sf_write_short  
	}
	else
	{
//printf("20 ");
		throw(CStdString("Write attempt on unopened file:")+ m_filename);
	}
//printf("___End LibSndFileFile::WrittChunk\n");
}

int LibSndFileFile::ReadChunkMono(AudioChunkRef& chunk)
{
//printf("__Im LibSndFileFIle ReadChunkMono\n");
	unsigned int numRead = 0;
	if (m_pFile)
	{
//printf("__LibSndFileFile 1\n");
		chunk.reset(new AudioChunk());
		short temp[8000];
		numRead = sf_read_short(m_pFile, temp, 8000);
		AudioChunkDetails details;
		details.m_encoding = PcmAudio;
		details.m_numBytes = sizeof(short)*numRead;
		chunk->SetBuffer(temp, details);
	}
	else
	{
//printf("__LibSndFileFile 2\n");
		throw(CStdString("Read attempt on unopened file:")+ m_filename);
	}
	return numRead;
}


void LibSndFileFile::Close()
{
	if (m_pFile)
	{
		sf_close(m_pFile);
		m_pFile = NULL;
	}
}

CStdString LibSndFileFile::GetExtension()
{
	return ".wav";
}

void LibSndFileFile::SetNumOutputChannels(int numChan)
{
	m_numOutputChannels = numChan;
}

