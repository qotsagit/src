#include "serial_port.h" 
#if defined(_LINUX32) || defined (_LINUX64) 
#include <stdlib.h> 
#include <pthread.h> 
#include <unistd.h> 
#include <sys/file.h> 
//#include <errno.h>
//#include <linux/serial.h>
//#include <asm/termios.h>
#endif

int BaudRates[BAUD_LENGTH] = {2400,4800,9600,19200,38400,57600,115200};

#ifdef _WIN32
DWORD WINAPI CSerial::_W32Thread(void *Param)
#endif
#if defined(_LINUX32) || defined(_LINUX64)
void *_LINUXThread(void *Param)
#endif
{

    static int RetCode;
    CSerial *Serial = ((CSerial*)Param);
    Serial->OnBeforeMainLoop();
    Serial->SetWorkingFlag(true);
    Serial->Connect();
    
    while (true)
    {
        if (Serial->GetStop())
            break;

        if (Serial->IsConnected())
       	{

		if(!Serial->GetWriter())   // reader mode
		{
	   		RetCode = Serial->Read();
	    		Serial->SetLength(RetCode);

	    		if (RetCode > -1)
	    		{
				Serial->ResetErrors();
        		}else{
				if(Serial->GetReconnect())
        				Serial->SetIsConnected(false);
			}

		}

	}else{
		Serial->Reconnect();
	}


#ifdef _WIN32
        Sleep(THREAD_SLEEP);
#endif
#if defined(_LINUX32) || defined(_LINUX64)
      // usleep(THREAD_SLEEP * 1000);
#endif

    }

    Serial->OnAfterMainLoop();
    Serial->SetWorkingFlag(false);
    fprintf(stdout,"serial exit\n");
    pthread_exit(NULL);
}


CSerial::CSerial()
{
    m_Baud = BaudRates[0];
    m_CheckCRC = true;
	m_OpenPort = false;
	m_OldLineBuffer = NULL;
	m_Working = false;
	m_Writer = false;
	m_LineBuffer = NULL;
	m_ReconnectCounter = 0;
	m_PharseLine = false;
	//m_SerialBuffer = (char*)malloc(BUFFER);
	m_ComPort = NULL;
	m_Connected = false;
	m_NoNewSignalCounter = 0;
	m_NoNewSignal = false;
	m_LineBufferLength = 0;
	m_ReadTimeout = DEFAULT_READ_TIMEOUT;
	m_Reconnect = true;
	m_Flags = CRTSCTS | CS8 | CLOCAL | CREAD;
	m_CRCPos = DEFAULT_CRC_POS;
}

CSerial::~CSerial()
{
    m_Stop = true;
    m_Connected = false;
    vPorts.clear();
	ClearTalkers();
    ClearSignals();
    ClearLineBuffer();
}

void CSerial::SetCRCPos(size_t v)
{
    m_CRCPos = v;
}

void CSerial::SetPortFlags(int v)
{
	m_Flags = v;
}

bool CSerial::GetReconnect()
{
	return m_Reconnect;

}

void CSerial::SetReconnect(bool v)
{
	m_Reconnect = v;

}

void CSerial::SetReadTimeout(int v)
{
	m_ReadTimeout = v;
}

void CSerial::SetParseLine(bool value)
{
	m_PharseLine = value;
}

void CSerial::SetWriter(bool val)
{
	m_Writer = val;
}

bool CSerial::GetWriter()
{
	return m_Writer;
}

char *CSerial::GetLineBuffer()
{
	return m_LineBuffer;
}

size_t CSerial::GetLineBufferLength()
{
	return m_LineBufferLength;
}


size_t CSerial::GetLinesCount()
{
	return m_LinesCount;
}

size_t CSerial::GetSignalQuality()
{
	if(m_BadCrc == 0)
		return 100;

	float a = ((float)m_BadCrc / (float)m_LinesCount) * 100;
	return (size_t)100 - a;
}


void CSerial::SetCheckCRC(bool val)
{
	m_CheckCRC = val;
}

size_t CSerial::GetBadCRC()
{
	return m_BadCrc;
}

void CSerial::ClearSignals()
{
	for(size_t i = 0; i < vSignals.size(); i++)
	{
		free(vSignals[i].name);
		free(vSignals[i].nmea);
	}
	vSignals.clear();
}

void CSerial::ClearTalkers()
{
	for(size_t i = 0; i < vTalkers.size(); i++)
	{
		free(vTalkers[i].talker);

	}

	vTalkers.clear();
}

void CSerial::PharseLine( char *_Data, int _DataLen )
{

    int i = 0, Start = 0;
    int Len;
    char *DataPtr = (char*)_Data;

    int valid_nmea = 1;
  
    while( i < _DataLen )
    {

	if( memcmp(DataPtr, "\n", 1) == 0)
    {

		Len = i - Start;
		Len = Len + 1;
		if( m_OldLineLength == 0 )
		{
			m_LineBuffer = (char*)malloc(Len + 1);
			memset(m_LineBuffer,0,Len + 1);
            memcpy(m_LineBuffer, (_Data + Start ), Len);

		}else{

			m_LineBuffer = (char*)malloc(m_OldLineLength + Len + 1);
            memset(m_LineBuffer,0,m_OldLineLength + Len + 1);
            memcpy(m_LineBuffer, m_OldLineBuffer, m_OldLineLength);
            memcpy((m_LineBuffer + m_OldLineLength), (_Data + Start ), Len);
			ClearLineBuffer();
       }

		valid_nmea = NMEASignal((char*)m_LineBuffer);
		if(valid_nmea == 0)
			OnValidNMEA();
		if(valid_nmea == 1)
			OnInvalidNMEA();
		if(valid_nmea == 2)
			OnBadCRC(m_LineBuffer);	
		
		m_LineBufferLength = strlen((const char*)m_LineBuffer);
		OnLine((char*)m_LineBuffer, m_LineBufferLength);
	    
		free(m_LineBuffer);
		m_LineBuffer = NULL;
		DataPtr += m_EOLen;
		i += m_EOLen;
		Start += (Len + m_EOLen) - 1;

	}else{

            DataPtr++;
            i++;
        }

     }


    if( Start < _DataLen )          // bufor niedociętych linii
    {
		Len = _DataLen - Start;
		m_OldLineBuffer = (char*)realloc(m_OldLineBuffer, Len + m_OldLineLength);
		memcpy(m_OldLineBuffer, m_OldLineBuffer, m_OldLineLength);
		memcpy((m_OldLineBuffer + m_OldLineLength), (_Data + Start ), Len);
		m_OldLineLength += Len;
    }
	//fprintf(stdout,"old line lenght: %d\n",m_OldLineLength);
    if(m_OldLineLength > 128) // hardcoded here
		OnInvalidNMEA();
   
//    if(valid_plain_text)
//	OnValidPlainText();
//    else
//	OnInvalidPlainText();
	
}

int CSerial::NMEASignal(char *line)
{
	m_LinesCount++;

	if(!CheckCRC((const char*)line,m_CRCPos))
	{
		m_BadCrc++;
		if(m_CheckCRC)
		    return 2;
	}

	char *from = NULL;
	char *old_from = NULL;
	char *from1 = (char*)memchr(line,'!',strlen((char*)line));
	if(from1 != NULL)
		from = from1;

	char *from2 = (char*)memchr(line,'$',strlen((char*)line));
	if(from2 != NULL)
		from = from2;

	if(from == NULL)
		return 1;

	char *to = (char*)memchr(from,',',strlen((char*)from));
	if(to == NULL)
		return 1;
	
	if(from > to)
		return 1; 

	m_ValidNMEA = true;
	OnNMEALine(from,strlen((const char*)from));
	old_from = from;
#if 0
	AddTalker(from);
	AddSignal(from, to);
#endif	
	if(m_ValidNMEA)
		return 0;
	else
		return 1;

}


void CSerial::AddTalker(char *data)
{
	char *from = data;
	from = from + (1 * sizeof(char));
	char *buf = (char*)malloc( TALKER_LENGTH + 1 );
	memset(buf,0,TALKER_LENGTH + 1);
	memcpy(buf,from, TALKER_LENGTH);
	bool add = true;
	
	if(vTalkers.size() !=0)
	{	
		for( int i = 0; i < vTalkers.size();i++)
		{	
			if( memcmp(vTalkers[i].talker,buf,TALKER_LENGTH + 1) == 0)
			{
				add = false;
				break;
			}
		}
	}

	if(add)
	{	
		m_TalkerCounter = MAX_TALKER_COUNTER;
		STalker Talker;
		Talker.talker = buf;
		vTalkers.push_back(Talker);
		OnNewTalker(Talker);
	
	}else{

		m_TalkerCounter--;
		if(m_TalkerCounter <= 0 )
			OnNoNewTalker();
		free(buf);

	}
}

void CSerial::AddSignal(char *data, char *to)
{
	char *from = data;
	from = from + (3 * sizeof(char));
	char *buf = (char*)malloc( to - from + 1 );
	memset(buf,0,to - from + 1);
	memcpy(buf,from, to - from);
	bool add = true;
	
	if(vSignals.size() != 0)
	{
		for( int i = 0; i < vSignals.size();i++)
		{	
			if( memcmp(vSignals[i].name,buf,to - from) == 0)
			{
				add = false;
				vSignals[i].count++;
				break;
			}
		}
		
	}

	if(add)
	{	
		SSignal Signal;
		Signal.name = buf;
		Signal.count = 1;
		Signal.nmea = (char*)malloc(strlen((char*)from) + 1);
		strcpy((char*)Signal.nmea,(char*)from);
		vSignals.push_back(Signal);
		OnNewSignal(Signal);
				
	}else{
		
		free(buf);
	}

}
//bool CSerial::ValidPlainText(const char *line)
//{
    
//}

bool CSerial::CheckCRC(const char *nmea_line, size_t chcount) 
{
	const char *xor_chars = "0123456789ABCDEF";
	//size_t chcount = 1;
	int _xor = 0;

	size_t len_nmea = strlen(nmea_line);
	if(nmea_line[len_nmea - 5] != '*')
	    return false;

	while ( (chcount < len_nmea) && ( nmea_line[chcount] != '*') )
	_xor = _xor ^ (unsigned short int)(nmea_line[chcount++]);

	if ( nmea_line[chcount] != '*' ) return false;

	if( (nmea_line[chcount + 1] == xor_chars[_xor >> 4]) && (nmea_line[chcount + 2] == xor_chars[_xor & 0x0F]) )
	    return true;
	else
	    return false;
 
}


void CSerial::ClearLineBuffer(void)
{

    if( m_OldLineBuffer != NULL  )
    {
        free( m_OldLineBuffer );
        m_OldLineBuffer = NULL;
    }
    m_OldLineLength = 0;
}

void CSerial::ScanPorts()
{

    vPorts.clear();
    SPorts Ports;

#if defined(_WIN32) || defined(_WIN64)
    unsigned int port_number;
    for (port_number = 1; port_number < 257; port_number++)
    {

        //Form the Raw device name
        wchar_t port_name[32];
		//printf("checking port COM%d\n",port_number);
        swprintf(port_name,L"\\\\.\\COM%d",port_number);
        //Try to open the port
        bool bSuccess = false;
		//fwprintf(stderr,L"opening port %s\n",port_name);
        HANDLE hPort = CreateFile(port_name, GENERIC_READ , NULL, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (hPort == INVALID_HANDLE_VALUE)
        {
            DWORD dwError = GetLastError();
            //Check to see if the error was because some other app had the port open or a general failure
            //if (dwError == ERROR_ACCESS_DENIED || dwError == ERROR_GEN_FAILURE || dwError == ERROR_SHARING_VIOLATION || dwError == ERROR_SEM_TIMEOUT)
              //  bSuccess = true;
        }
        else
        {
            bSuccess = true;
            CloseHandle(hPort);
        }

        if (bSuccess)
        {
            char value_1[PORT_NAME_LENGTH];
            memset(value_1,0,PORT_NAME_LENGTH);
            sprintf_s(value_1,"COM%d",port_number);
            strcpy_s(Ports.port_name,value_1);

            char value_2[PORT_STRING_LENGTH];
            memset(value_2,0,PORT_STRING_LENGTH);
            sprintf_s(value_2,"\\\\.\\COM%d",port_number);
            strcpy_s(Ports.port_string,value_2);

            Ports.port_number = port_number;
            vPorts.push_back(Ports);
        }
    }
#endif

#if defined(_LINUX32) || (_LINUX64)
    unsigned int port_number;

    for (port_number = 0; port_number < 257; port_number++)
    {
        //if(m_Stop)
        //    return;

        char port_name[PORT_NAME_LENGTH];
        bool bSuccess = false;
        memset(port_name,'0',PORT_NAME_LENGTH);

        // dla ttyS.............................................//
        sprintf(port_name,"/dev/ttyUSB%d",port_number);
    	//printf("%s",port_name);
        bSuccess = Scan(port_name);

        if (bSuccess)
        {
            memset(Ports.port_name,'0',PORT_NAME_LENGTH);
            strcpy(Ports.port_name,port_name);
            memset(Ports.port_string,'0',PORT_STRING_LENGTH);
            strcpy(Ports.port_string,port_name);
            Ports.port_number = port_number;
            vPorts.push_back(Ports);
        }
        //..........................................................

        // dla ttyS.................................................//
        memset(port_name,'0',PORT_NAME_LENGTH);
        sprintf(port_name,"/dev/ttyS%d",port_number);

        bSuccess = Scan(port_name);
        if (bSuccess)
        {
            //fprintf(stderr,"Dodano:%s\n",port_name);
            memset(Ports.port_name,'0',PORT_NAME_LENGTH);
            strcpy(Ports.port_name,port_name);
            memset(Ports.port_string,'0',PORT_STRING_LENGTH);
            strcpy(Ports.port_string,port_name);
            Ports.port_number = port_number;
            vPorts.push_back(Ports);
        }
        //..........................................................
    }

#endif

    if (vPorts.size() == 0)
        m_NumberOfPorts = -1;
    else
        m_NumberOfPorts = vPorts.size() - 1;

}
#if defined(_LINUX32) || defined(_LINUX64)
bool CSerial::Scan(char port_name[PORT_NAME_LENGTH])
{
    int fdesc;
    struct termios port_settings;
    int error = -1;
    struct flock fl;

    fdesc = open(port_name, O_RDWR | O_NOCTTY |O_NDELAY);
    if (fdesc < 0)
    {
        //printf("unable to open \n");
        return false;
    }

    error = tcgetattr(fdesc, &port_settings);
    if(error==-1)
    {
        //printf("unable to get attr \n");
        close(fdesc);
        return false;
    }
    error = flock(fdesc,LOCK_EX | LOCK_NB);

    if(error < 0)
    {
        close(fdesc);
        return false;
    }

    close(fdesc);
    return true;
}
#endif
size_t CSerial::GetPortInfoLength()
{
	return vPorts.size();
}

SPorts CSerial::GetPortInfo(int id)
{
    return vPorts[id];
}

size_t CSerial::GetBaudInfoLength()
{
	return BAUD_LENGTH;
}

int CSerial::GetBaudInfo(int id)
{
	return 	BaudRates[id];
}

bool CSerial::GetStop()
{
    return m_Stop;
}

void CSerial::Stop(bool wait)
{
    m_Stop = true;
    OnStop();
    
    while(m_Working)
    {
	usleep(100 * 1000);
	fprintf(stderr,"Waiting!\n");
    }
    
    if (m_OpenPort) // wa¿ne w tym miejscu po fladze stop
    {
		m_OpenPort = false;
#if defined(_WIN32) || defined(_WIN64)
		LPDWORD lpErrors = 0;
		//CancelIo(m_ComPort);
		ClearCommError(m_ComPort,lpErrors,NULL);
		CloseHandle(m_ComPort);
		m_ComPort = NULL;
#endif
#if defined(_LINUX32) || defined(_LINUX64)
		flock(m_ComPort,LOCK_UN);
		tcsetattr(m_ComPort, TCSANOW, &m_OldPortSettings);
		fprintf(stderr,"Begin Close form stopn");
		int a = close(m_ComPort);
		fprintf(stderr,"END Close form stop %d\n",a);
		//m_ComPort = -1;
#endif
	  
	}

#if defined(_WIN32) || defined(_WIN64)
	if(wait)
		WaitForSingleObject(m_ThreadHANDLE,INFINITE);
#endif
   
    m_BadCrc = 0;
    m_LinesCount = 0;
    m_OldLineLength = 0;
    m_EOLen = EOL_LENGTH;
    m_EmptyCount = 0;
    m_Connected = false;
    m_NumberOfPorts = -1;
    m_OpenPort = false;
    m_ValidDevice = false;
    m_Errors = 0;
    m_ComPort = NULL;
    m_ValidNMEA = false;
    m_BufferLength = -1;
	m_NoNewSignal = false;
    vPorts.clear();
	ClearLineBuffer();
}

void CSerial::Start()
{
    if (m_Working)
        return;

    m_BadCrc = 0;
    m_LinesCount = 0;
    m_OldLineBuffer = NULL;
    m_OldLineLength = 0;
    m_EOLen = EOL_LENGTH;
    m_EmptyCount = 0;
    m_Connected = false;
    m_NumberOfPorts = -1;
    m_OpenPort = false;
    m_Working = false;
    m_ValidDevice = false;
    m_Errors = 0;
    m_Stop = false;
    //m_FirstTime = true;
    m_ComPort = NULL;
    m_ValidNMEA = false;
    m_BufferLength = -1;
    vPorts.clear();
	ClearSignals();
	ClearTalkers();
    
	OnStart();

    StartThread();

}

bool CSerial::GetWorkingFlag()
{
    return m_Working;
}

void CSerial::SetWorkingFlag(bool work)
{
    m_Working = work;
}

void CSerial::SetStop(bool v)
{
	m_Stop = v;
}

const char *CSerial::GetPortName()
{
	return m_Port;
}

bool CSerial::Connect(const char *port, int baud_rate)
{
    if (m_Stop) return false;

    m_BadCrc = 0;
    m_LinesCount = 0;
    m_Baud = baud_rate;
    memcpy(m_Port,port,PORT_NAME_LENGTH);
    OnConnect();
    OpenPort(port,baud_rate);

#if defined(_WIN32) || defined(_WIN64)
    if (m_ComPort != NULL)
    {
#endif
#if defined(_LINUX32) || defined(_LINUX64)
    if (m_ComPort > -1)
    {
#endif
        m_Connected = true;
        m_OpenPort = true;

		OnConnected();

    }else{

        m_Connected = false;
        OnDisconnect();
    }


    return m_Connected;

}
void CSerial::Disconnect()
{
#if defined(_WIN32) || defined(_WIN64)
	if(m_ComPort)
		CloseHandle(m_ComPort);
	m_ComPort = NULL;
#endif
#if defined(_LINUX32) || defined(_LINUX64)    
	flock(m_ComPort,LOCK_UN);
	//tcsetattr(m_ComPort, TCSANOW, &m_OldPortSettings);
    int ret = close(m_ComPort);
    //fprintf(stderr,"Close %s %d %d\n",m_Port,m_Baud, ret);
    m_ComPort = -1;
#endif
    m_Connected = false;
    m_OpenPort = false;

}
bool CSerial::Connect()
{
	return Connect(m_Port,m_Baud);
}

bool CSerial::Reconnect()
{
	if (m_Stop)
        	return false;
	if(!m_Reconnect)
		return false;
	bool con = false;
	m_ReconnectCounter++;
	if(m_ReconnectCounter < RECCONNECT_COUNTER)
		return false;

	OnReconnect();

#if defined(_WIN32) || defined(_WIN64)
	//Sleep(100);
#endif
#if defined(_LINUX32) || defined(_LINUX64)
    if(m_OpenPort)
    {
	flock(m_ComPort,LOCK_UN);
	int ret = close(m_ComPort);
	m_ComPort = -1;
    }
    sleep(1);
#endif

	con = Connect(m_Port,m_Baud);
	m_ReconnectCounter = 0;

    //m_FirstTime = false;
    return con;
}

void CSerial::_SetPort(const char *port)
{
	strcpy(m_Port,port);
}

void CSerial::SetBaud(int baud)
{
    //m_FirstTime = true;
    m_Baud = baud;
}

bool CSerial::IsConnected()
{
    //fprintf(stderr,"IsConnected %d",m_Connected);
    return m_Connected;
}

void CSerial::SetIsConnected(bool val)
{
     m_Connected = false;
}

size_t CSerial::GetTalkerCount()
{
	return vTalkers.size();
}

char *CSerial::GetTalker(size_t id)
{
	return vTalkers[id].talker;
}

size_t CSerial::GetSignalCount()
{
	return vSignals.size();
}

SSignal *CSerial::GetSignal(int idx)
{
	return &vSignals[idx];
}

int CSerial::Read()
{
    struct timeval tv;
    int retval;
    m_ValidNMEA = false;
    memset(m_SerialBuffer,0,BUFFER_LENGTH);
#if defined(_LINUX32) || defined(_LINUX64)
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_ComPort, &rfds);

    tv.tv_sec = 0;
    tv.tv_usec = m_ReadTimeout * 1000;
    retval = select(m_ComPort,&rfds,NULL,NULL,&tv);
    //if(!retval)
    //{
	//fprintf(stderr,"%s RETVA[%d]\n",GetPortName(),retval);
//	return -1;
  //  }
#endif

    m_BufferLength = ReadPort(m_ComPort,m_SerialBuffer,BUFFER_LENGTH);
    //fprintf(stderr,"BUFFER:%d\n",m_BufferLength);

    if(m_BufferLength == 0)
    	m_EmptyCount++;
    else
    	m_EmptyCount = 0;

    if(m_EmptyCount >= _MAX_ZERO_COUNTER)
    {
		OnNoSignal();
		m_EmptyCount = 0;
		//Disconnect();
    }


	if(m_BufferLength > 0)
    {
	OnData(m_SerialBuffer,m_BufferLength);
	if(m_PharseLine)
    	    PharseLine((char*)m_SerialBuffer,m_BufferLength);

    }

    return m_BufferLength;
}

int CSerial::Write(char *buffer, int length)
{
    int size = 0;
#if defined(_WIN32) || defined(_WIN64)
    size = WritePort(m_ComPort,buffer,length);
#endif
#if defined(_LINUX32) || defined(_LINUX64)
    size = WritePort(m_ComPort,buffer,length);
#endif
    m_LinesWritten++;
    return size;
}
int CSerial::GetLinesWriten()
{
	return m_LinesWritten;
}

void CSerial::SetLength(int size)
{
    m_BufferLength = size;
}

int CSerial::GetBaudRate()
{
    return m_Baud;
}

int CSerial::GetErrors()
{
    //fprintf(stderr,"Errors %d\n",m_Errors);
    return m_Errors;
}

void CSerial::IncrementErrors()
{
    //fprintf(stderr,"Increment %d\n",m_Errors);
    m_Errors++;
}

void CSerial::ResetErrors()
{
    m_Errors = 0;
}

char *CSerial::GetBuffer()
{
    return m_SerialBuffer;
}

int CSerial::GetLength()
{
    return m_BufferLength;
}

#if defined(_WIN32) || defined(_WIN64)
HANDLE CSerial::GetHandle()
{
    return m_ThreadHANDLE;
}
#endif

void CSerial::StartThread()
{

#if defined (_WIN32) || defined(_WIN64)
    m_ThreadHANDLE = CreateThread( NULL, 0, _W32Thread, this, 0, &threadID);
#endif
#if defined(_LINUX32) || defined(_LINUX64)
    pthread_t thr;
    int ret = 0;
    ret = pthread_create(&thr,NULL,&_LINUXThread,(void*)this);
    pthread_detach( thr );
#endif
}


bool CSerial::GetValidNMEA()
{
    return m_ValidNMEA;
}

#ifdef __linux__   /* Linux */
#include <sys/file.h>
#include <errno.h>


void CSerial::OpenPort(const char *port, int baudrate)
{
    int baudr;
    int error;
    struct flock fl;
    struct termios port_settings;

    switch(baudrate)
    {

    case    2400 : baudr = B2400; break;
    case    4800 : baudr = B4800; break;
    case    9600 : baudr = B9600; break;
    case   19200 : baudr = B19200;break;
    case   38400 : baudr = B38400;break;
    case   57600 : baudr = B57600;break;
    case  115200 : baudr = B115200;break;
    default      : return; break;
    }

    m_ComPort = open(port, O_RDWR | O_NOCTTY | O_NDELAY );

    if(m_ComPort==-1)
    {
        fprintf(stderr,"Open port er1");
        return;
    }
    error = flock(m_ComPort, LOCK_EX | LOCK_NB );
    if(error==-1)
    {
        close(m_ComPort);
        fprintf(stderr,"Open port er2");
        return;
    }

    error = tcgetattr(m_ComPort, &m_OldPortSettings);
    if(error==-1)
    {
        close(m_ComPort);
        fprintf(stderr,"Open port er3");
        return;
    }

    memset(&port_settings, 0, sizeof(port_settings));  /* clear the new struct */
    //port_settings = m_OldPortSettings;
    
    //	port_settings.c_cflag = baudr | CRTSCTS | CS8 | CLOCAL | CREAD;
    //else
    port_settings.c_cflag =  baudr | m_Flags;
    port_settings.c_iflag = IGNPAR;
    port_settings.c_oflag = 0;
    port_settings.c_cc[VMIN] = 0;      /* block untill n bytes are received */
    port_settings.c_cc[VTIME] =  0;     /* block untill a timer expires (n * 100 mSec.) */
    error = tcsetattr(m_ComPort, TCSANOW, &port_settings);

    
    if(error==-1)
    {
        close(m_ComPort);
        fprintf(stderr,"Open port er4");
        return;
    }
    
    
    //struct serial_struct serial;
    //ioctl(m_ComPort, TIOCGSERIAL, &serial); 
    //serial.flags |= ASYNC_LOW_LATENCY; // (0x2000)
    //ioctl(m_ComPort, TIOCSSERIAL, &serial);

    return;
}


int CSerial::ReadPort(int ComPort,char *buf, int size)
{
    int n;
    n = read(ComPort, buf, size);
    //if(n > 0)
	//fprintf(stderr,"readport:%d\n",n);
    return n;
}

int CSerial::WritePort(int ComPort, char *buf, int size)
{
    int n;
    n = write(ComPort, buf, size);
    //fprintf(stderr,"[%d] [%d]\n ",size, n);
    return(n);
}


//void CSerial::ClosePort()
//{
//    flock(ComPort,LOCK_UN);
//    close(ComPort);
//    tcsetattr(ComPort, TCSANOW, &old_port_settings);
//}

/*
Constant  Description
TIOCM_LE  DSR (data set ready/line enable)
TIOCM_DTR DTR (data terminal ready)
TIOCM_RTS RTS (request to send)
TIOCM_ST  Secondary TXD (transmit)
TIOCM_SR  Secondary RXD (receive)
TIOCM_CTS CTS (clear to send)
TIOCM_CAR DCD (data carrier detect)
TIOCM_CD  Synonym for TIOCM_CAR
TIOCM_RNG RNG (ring)
TIOCM_RI  Synonym for TIOCM_RNG
TIOCM_DSR DSR (data set ready)
*/

//int IsCTSEnabled(int comport_number)
//{
//  int status;

//  status = ioctl(ComPort[comport_number], TIOCMGET, &status);

//  if(status&TIOCM_CTS) return(1);
//  else return(0);
//}


#else         /* windows */




void CSerial::OpenPort(const char *port, int baudrate)
{
	/*
	char baudr[64];

	switch(baudrate)
    {

    case    2400 :
        strcpy_s(baudr, "baud=2400 data=8 parity=N stop=1");
        break;
    case    4800 :
        strcpy_s(baudr, "baud=4800 data=8 parity=N stop=1");
        break;
    case    9600 :
        strcpy_s(baudr, "baud=9600 data=8 parity=N stop=1");
        break;
    case   19200 :
        strcpy_s(baudr, "baud=19200 data=8 parity=N stop=1");
        break;
    case   38400 :
        strcpy_s(baudr, "baud=38400 data=8 parity=N stop=1");
        break;
    case   57600 :
        strcpy_s(baudr, "baud=57600 data=8 parity=N stop=1");
        break;
    case  115200 :
        strcpy_s(baudr, "baud=115200 data=8 parity=N stop=1");
        break;
    default      :
        printf("invalid baudrate\n");
		return;
        break;
    }
	*/
	int len = strlen(port) + 1 + 7;
	char *port_string = (char*)malloc(len);
	memset(port_string,0,len);

	sprintf(port_string,"\\\\.\\%s",port);
    //m_ComPort = CreateFileA(port_string, GENERIC_READ|GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	m_ComPort = CreateFileA(port_string, GENERIC_READ|GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);

	free(port_string);
    
	if(m_ComPort == INVALID_HANDLE_VALUE)
    {	
		m_ComPort = NULL;
        return;
    }

    DCB port_settings;
    //memset(&port_settings, 0, sizeof(port_settings));  /* clear the new struct  */
   //port_settings.DCBlength = sizeof(port_settings);

	if(!GetCommState(m_ComPort,&port_settings))
	{
	    CloseHandle(m_ComPort);
		m_ComPort = NULL;
		return;
	}

    //if(!BuildCommDCBA(baudr, &port_settings))
    //{
        //CloseHandle(m_ComPort);
		//m_ComPort = NULL;
		//return;
    //}

	port_settings.BaudRate = baudrate;
    if(!SetCommState(m_ComPort, &port_settings))
    {
        CloseHandle(m_ComPort);
		m_ComPort = NULL;
		return;
    }

    COMMTIMEOUTS Cptimeouts;

	Cptimeouts.ReadIntervalTimeout         = MAXDWORD;
    Cptimeouts.ReadTotalTimeoutMultiplier  = 0;
	Cptimeouts.ReadTotalTimeoutConstant    = 10;
    Cptimeouts.WriteTotalTimeoutMultiplier = 0;
    Cptimeouts.WriteTotalTimeoutConstant   = 0;

    if(!SetCommTimeouts(m_ComPort, &Cptimeouts))
    {
        CloseHandle(m_ComPort);
		m_ComPort = NULL;
        return;
    }

	return;
}


int CSerial::ReadPort(HANDLE port,char *buf, int size)
{
	if(m_Stop)
		return -1;

	int nBytesRead = 0;
	if(size > 4096)  
		size = 4096;

	int t = GetTickCount();
	int timeout = 10;
	BOOL bResult;
	COMSTAT status;
	DWORD errors;
	memset(m_SerialBuffer,0,BUFFER_LENGTH);
<<<<<<< HEAD

	ClearCommError(port, &errors, &status);
=======
>>>>>>> c74c5deaa0bebbdd9bbc1649b603f3a0c7772e6f

	//Check if there is something to read
	int counter = 2;
	while(counter--)
	{

	//ClearCommError(port, &errors, &status);
	//if (status.cbInQue > 0)
	//{
			bResult = ReadFile(port, buf, size, (LPDWORD)&nBytesRead, NULL);
			if(bResult && nBytesRead > 0)
				return nBytesRead;
			//else
				//return  0;
		
	}	
<<<<<<< HEAD

=======
	
	//fprintf(stderr,"Queue empty\n");
>>>>>>> c74c5deaa0bebbdd9bbc1649b603f3a0c7772e6f
	return -1;
}


/*
int CSerial::ReadPort(HANDLE port, char *buf, int size)
{
	
    int nBytesRead = 0;
    DWORD dwCommModemStatus,deRes;
    OVERLAPPED o;
    if(size>4096)  size = 4096;
*/
    /* added the void pointer cast, otherwise gcc will complain about */
    /* "warning: dereferencing type-punned pointer will break strict aliasing rules" */
/*
    SetCommMask (port, EV_RXCHAR|EV_CTS|EV_DSR|EV_RING|EV_ERR|EV_BREAK|EV_RLSD);
	o.hEvent = CreateEvent(
                   NULL,   // default security attributes
                   TRUE,   // manual-reset event
                   FALSE,  // not signaled
                   NULL    // no name
               );

	if(o.hEvent == NULL)
		return 0;
    
	// Initialize the rest of the OVERLAPPED structure to zero.
    o.Internal = 0;
    o.InternalHigh = 0;
    o.Offset = 0;
    o.OffsetHigh = 0;
	o.Pointer = 0;
		
    bool waitRes = WaitCommEvent (port, &dwCommModemStatus, &o);
	if(!waitRes)
	{
		if(GetLastError() != ERROR_IO_PENDING)
			return -2;
	}

	deRes = WaitForSingleObject(o.hEvent,1000);
    
	switch(deRes)
    {
    
		case WAIT_OBJECT_0:
		{
			BOOL bResult = ReadFile(port, buf, size, (LPDWORD)&nBytesRead, &o);
			if(bResult)
			{
				CloseHandle(o.hEvent);
				return nBytesRead;
			}else{
				DWORD lError = GetLastError();
				
				if (lError != ERROR_IO_PENDING)
					nBytesRead = 0;

				GetOverlappedResult(port,&o,(LPDWORD)&nBytesRead,FALSE);
			}
		
			break;
		}
    
		case WAIT_TIMEOUT:
			nBytesRead = 0;
			CancelIo(port);
			break;
		
		case WAIT_FAILED:
			nBytesRead = -1;
			break;
	}
	
	
	
    return nBytesRead;
}
*/

/*
int CSerial::WritePort(HANDLE port,char *buf, int size)
{
	int n = -1;
    if(port == INVALID_HANDLE_VALUE)
		return -1;

	OVERLAPPED o;
	o.hEvent = CreateEvent(
                   NULL,   // default security attributes
                   TRUE,   // manual-reset event
                   FALSE,  // not signaled
                   NULL    // no name
               );

	if(o.hEvent == NULL)
		return 0;
    
	// Initialize the rest of the OVERLAPPED structure to zero.
    o.Internal = 0;
    o.InternalHigh = 0;
    o.Offset = 0;
    o.OffsetHigh = 0;
	o.Pointer = 0;

	BOOL bResult = WriteFile(port, buf, size, (LPDWORD)((void *)&n), &o);

	if(bResult)
	{
			CloseHandle(o.hEvent);
			return n;
	}else{

		DWORD lError = GetLastError();

		if (lError != ERROR_IO_PENDING)
			n = 0;

		GetOverlappedResult(port,&o,(LPDWORD)&n,FALSE);
	}

	//CancelIo(port);

	return n;
}
*/

int CSerial::WritePort(HANDLE port,char *buf, int size)
{
	//return -1;
    int n = -1;
    if(port == INVALID_HANDLE_VALUE)
		return -1;

	BOOL bResult = WriteFile(port, buf, size, (LPDWORD)((void *)&n), NULL);
	if(bResult)
		return n;
	else
		n = 0;

	return n;
}
#endif

// virtual methods
void CSerial::OnConnect()
{

}
void CSerial::OnConnected()
{

}
void CSerial::OnDisconnect()
{
}
void CSerial::OnLine( char* buffer,int length)
{
}
void CSerial::OnNMEALine( char* buffer,int length)
{
}
void CSerial::OnData( char* buffer, int length)
{
}
void CSerial::OnExit()
{
}
void CSerial::OnStart()
{
}
void CSerial::OnStop()
{
}
void CSerial::OnBeforeMainLoop()
{
}
void CSerial::OnAfterMainLoop()
{
}
void CSerial::OnReconnect()
{
}
void CSerial::OnNewSignal(SSignal signal)
{
}
void CSerial::OnNoSignal()
{
}
void CSerial::OnValidNMEA()
{
}
void CSerial::OnInvalidNMEA()
{
}
void CSerial::OnBadCRC(char *line)
{
}
void CSerial::OnNewTalker(STalker talker)
{

}
void CSerial::OnNoNewTalker()
{

}

//void CSerial::OnInvalidPlainText()
//{
//}
