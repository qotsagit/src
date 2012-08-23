#include "serial_port.h"
#if defined(_LINUX32) || defined (_LINUX64)
#include <pthread.h>
#include <unistd.h>
#include <sys/file.h>
#include <errno.h>
#endif

int BaudRates[BAUD_LENGTH] = {115200,57600,38400,9600,4800};
//int PortNumbers[NUMBER_OF_PORTS];

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
    while (true)
    {

        if (Serial->GetStop())
            break;


        if (Serial->GetIsConnected())
        {
            RetCode = Serial->Read();
            Serial->SetLength(RetCode);

            if (RetCode > -1) // all ok
            {
                Serial->SetnErrors(0);
                Serial->OnData();
            
			}else{
                
				Serial->Reconnect();
            }

        }else{
            
			Serial->Reconnect();
			
        }


#ifdef _WIN32
     //   Sleep(500);
#endif
#if defined(_LINUX32) || defined(_LINUX64)
        sleep(1);
#endif

    }
    Serial->OnAfterMainLoop();
	Serial->SetWorkingFlag(false);
    

return 0;
}


CSerial::CSerial(int device_type)
{

	m_emptyCount = 0;
    m_DeviceType = device_type;
    m_Connected = false;
    m_NumberOfPorts = -1;
    m_BaudIndex = 0;
    m_PortIndex = 0;
    m_OpenPort = false;
    m_Working = false;
    m_ValidDevice = false;
    m_nErrors = 0;
    m_Stop = false;
	m_FirstTime = true;
	m_ComPort = NULL;
	vPorts.clear();
	BuildPorts();

}

CSerial::~CSerial()
{
    m_Stop = true;
    m_Working = false;
    m_Connected = false;
    if (m_OpenPort)
    {
		CloseHandle(m_ComPort);
		m_ComPort = NULL;
        m_OpenPort = false;
    }
	vPorts.clear();
	m_PortIndex = 0;
}
void CSerial::BuildPorts()
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
        wsprintf(port_name,L"\\\\.\\COM%d",port_number);
        //Try to open the port
        bool bSuccess = false;
        HANDLE hPort = CreateFile(port_name, GENERIC_READ , 0, 0, OPEN_EXISTING, 0, 0);
        if (hPort == INVALID_HANDLE_VALUE)
        {
            DWORD dwError = GetLastError();
            //Check to see if the error was because some other app had the port open or a general failure
            if (dwError == ERROR_ACCESS_DENIED || dwError == ERROR_GEN_FAILURE || dwError == ERROR_SHARING_VIOLATION || dwError == ERROR_SEM_TIMEOUT)
                bSuccess = true;
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
        if(m_Stop)
            return;

        char port_name[PORT_NAME_LENGTH];
        bool bSuccess = false;
        memset(port_name,'0',PORT_NAME_LENGTH);

        // dla ttyS.............................................//
        sprintf(port_name,"/dev/ttyUSB%d",port_number);
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
    //error = flock(fdesc,LOCK_EX | LOCK_NB);
    //if(error == EWOULDBLOCK)
    //{
    //    close(fdesc);
    //    return false;
    //}

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

void CSerial::Stop()
{

    m_Stop = true;
#if defined(_WIN32) || defined(_WIN64)
    WaitForSingleObject(m_ThreadHANDLE,INFINITE);
#endif
#if defined(_LINUX32) || defined(_LINUX64)
    sleep(1);
#endif

    OnStop();

    if (m_OpenPort) // wa¿ne w tym miejscu po fladze stop
    {
        CloseHandle(m_ComPort);
		m_ComPort = NULL;
        m_OpenPort = false;
    }
    m_Connected = false;
    m_Working = false;
    m_ValidDevice = false;
	memset(m_SerialBuffer,0,BUFFER);
	m_BufferLength = 0;

}

void CSerial::Start()
{
    if (m_Working)
        return;

    OnStart();
    m_Stop = false;
	
	if(m_FirstTime && m_PortIndex == 0)
	{
		m_PortIndex = 0;
		m_BaudIndex = 0;
		m_NumberOfPorts = -1;
		
	}
	
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
//void CSerial::SetNumberOfPorts(int val)
//{
//    m_NumberOfPorts = val;
//}


char *CSerial::GetPortName()
{
	if(vPorts.size() > 0)
		return vPorts[m_PortIndex].port_name;
	else
		return NULL;
}

bool CSerial::Connect(char port_string[PORT_STRING_LENGTH], int baud_rate)
{
    if (m_Stop) return false;

    m_OpenPort = false;
    m_BaudRate = baud_rate;

	OpenPort(port_string,baud_rate);

    if (m_ComPort != NULL)
    {
        m_Connected = true;
        m_OpenPort = true;
        OnConnect();
    }
    else
    {
        m_Connected = false;
        OnDisconnect();
    }
	
    return m_Connected;


}

bool CSerial::Reconnect()
{

    if (m_Stop)
        return false;


    if (m_NumberOfPorts == -1)
    {
        BuildPorts();
        OnNewScan();
        return false;
    }


    if (m_OpenPort)
    {
        //ClosePort(_SerialPort);
		if(m_ComPort != INVALID_HANDLE_VALUE)
			CloseHandle(m_ComPort);
		m_ComPort = NULL;
        m_OpenPort = false;
    }

    m_ValidDevice = false;

    if (m_BaudIndex >=  BAUD_LENGTH && !m_FirstTime)
    {
		
        m_BaudIndex = 0;
        m_nErrors++;
        m_PortIndex++;

        if (m_PortIndex > m_NumberOfPorts)
        {
            m_PortIndex = 0;			// zacznij od pierwszego portu w tablicy portów
            BuildPorts();
            OnNewScan();
        }
        else
        {
            // -1 brak portów w systemie
            return false;
        }
    }

	bool con = false;
	m_FirstTime = false;
	if((int)vPorts.size() > m_PortIndex)
	{
		
		con = Connect(vPorts[m_PortIndex].port_string,BaudRates[m_BaudIndex]);
		
		OnReconnect();
	}
    
	m_BaudIndex++;
	
    return con;
}
void CSerial::SetPortIndex(int id)
{
	m_PortIndex = id;
	if(id < 0)
		m_PortIndex = 0;
}

void CSerial::SetBaudIndex(int id)
{
	m_BaudIndex = id;
	if(id < 0)
		m_BaudIndex = 0;
}


int CSerial::GetPortIndex()
{
	return m_PortIndex;
}

int CSerial::GetBaudIndex()
{
	return m_BaudIndex - 1;
}


bool CSerial::IsValidDevice()
{
    if (m_Stop) return false;

    if (m_ValidDevice)
        return true;

    if (GetDeviceType() == m_DeviceType)
    {
        OnValid();
        m_ValidDevice = true;
        return true;
    }
    else
    {
        OnInvalid();
        m_ValidDevice = false;
        return false;
    }

}

bool CSerial::GetIsConnected()
{
    return m_Connected;
}

int CSerial::Read()
{
    memset(m_SerialBuffer,0,BUFFER);
    m_BufferLength = ReadPort(m_ComPort,m_SerialBuffer,BUFFER);
	
	if(m_BufferLength == 0)
		m_emptyCount++;
	else
		m_emptyCount = 0;
	
	if(m_emptyCount >= 10)
	{
		m_BufferLength = -1;
		m_emptyCount = 0;
	}
	//printf("%d\n",m_BufferLength);
	printf("%d",m_emptyCount);
	return m_BufferLength;
}

int CSerial::GetDeviceType()
{
	   
	char *find = NULL;
    int from = 0;
    
	if(m_BufferLength < 0)
		return -1;
    
	//printf("%s %d\n",m_SerialBuffer,m_BufferLength);

	switch(m_DeviceType)
    {

        case DEVICE_AIS:
            find = (char*)memchr(m_SerialBuffer,'!',m_BufferLength);
        break;

        case DEVICE_GPS:
            find = (char*)memchr(m_SerialBuffer,'$',m_BufferLength);
        break;
    }


    if (find == NULL)
    {
        find = (char*)memchr(m_SerialBuffer,'\n',m_BufferLength);
        if (find == NULL)
            return -1;
        else
            from = 1;

    }
    else
    {
        from = 0;
    }

    if (memcmp(find + from,_AIS,3)==0)
        return DEVICE_AIS;
    if (memcmp(find + from,_GPSAIS,3)==0)
        return DEVICE_AIS;
    if (memcmp(find + from,_GPS,3)==0)
        return DEVICE_GPS;

    return -1;
}

void CSerial::SetLength(int size)
{
    m_BufferLength = size;
}

//bool CSerial::GetOpenPort()
//{
//    return m_OpenPort;
//}

//int CSerial::GetPortNumber()
//{
//    return m_Port;
//}

int CSerial::GetBaudRate()
{
    return m_BaudRate;
}

int CSerial::GetnErrors()
{
    return m_nErrors;
}

void CSerial::SetnErrors(int n)
{
    m_nErrors = n;
}

unsigned char *CSerial::GetBuffer()
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

void CSerial::Disconnect()
{
}


#ifdef __linux__   /* Linux */
#include <sys/file.h>
#include <errno.h>


int ComPort;
struct termios old_port_settings;

int OpenPort(char port[16], int baudrate)
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
    default      : return(1); break;
    }

    ComPort = open(port, O_RDWR | O_NOCTTY | O_NDELAY );

    if(ComPort==-1)
        return 1;

    error = flock(ComPort, LOCK_EX | LOCK_NB );
    if(error==-1)
    {
        close(ComPort);
        return 1;
    }

    error = tcgetattr(ComPort, &old_port_settings);
    if(error==-1)
    {
        close(ComPort);
        return 1;
    }

    memset(&port_settings, 0, sizeof(port_settings));  /* clear the new struct */
    port_settings.c_cflag = baudr | CRTSCTS | CS8 | CLOCAL | CREAD;
    port_settings.c_iflag = IGNPAR;
    port_settings.c_oflag = 0;
    port_settings.c_cc[VMIN] = 0;      /* block untill n bytes are received */
    port_settings.c_cc[VTIME] = 0;     /* block untill a timer expires (n * 100 mSec.) */
    error = tcsetattr(ComPort, TCSANOW, &port_settings);

    if(error==-1)
    {
        close(ComPort);
        return 1;
    }

    return 0;
}


int ReadPort(unsigned char *buf, int size, bool check)
{
    int n;

#ifndef __STRICT_ANSI__                       /* __STRICT_ANSI__ is defined when the -ansi option is used for gcc */
    if(size>SSIZE_MAX)  size = (int)SSIZE_MAX;  /* SSIZE_MAX is defined in limits.h */
#else
    if(size>4096)  size = 4096;
#endif

    n = read(ComPort, buf, size);

    return(n);
}

void ClosePort_()
{
    flock(ComPort,LOCK_UN);
    close(ComPort);
    tcsetattr(ComPort, TCSANOW, &old_port_settings);
}

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




void CSerial::OpenPort(char port[16], int baudrate)
{
	
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
	
    m_ComPort = CreateFileA(port, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

    if(m_ComPort == INVALID_HANDLE_VALUE)
    {	
		m_ComPort = NULL;
        //printf("unable to open comport\n");
		return;
    }

	
    DCB port_settings;
    memset(&port_settings, 0, sizeof(port_settings));  /* clear the new struct  */
    port_settings.DCBlength = sizeof(port_settings);

    if(!BuildCommDCBA(baudr, &port_settings))
    {
        //printf("unable to set comport dcb settings\n");
        CloseHandle(m_ComPort);
		m_ComPort = NULL;
		return;
    }
	
    if(!SetCommState(m_ComPort, &port_settings))
    {
        //printf("unable to set comport cfg settings\n");
        CloseHandle(m_ComPort);
		m_ComPort = NULL;
		return;
    }
	
    COMMTIMEOUTS Cptimeouts;

    Cptimeouts.ReadIntervalTimeout         = MAXDWORD;
    Cptimeouts.ReadTotalTimeoutMultiplier  = 0;
    Cptimeouts.ReadTotalTimeoutConstant    = 0;
    Cptimeouts.WriteTotalTimeoutMultiplier = 0;
    Cptimeouts.WriteTotalTimeoutConstant   = 0;

    if(!SetCommTimeouts(m_ComPort, &Cptimeouts))
    {
        //printf("unable to set comport time-out settings\n");
        CloseHandle(m_ComPort);
		m_ComPort = NULL;
        return;
    }
	
	return;
}


int CSerial::ReadPort(HANDLE port,unsigned char *buf, int size)
{
	
    int nBytesRead = 0;
    DWORD dwCommModemStatus,deRes;
    OVERLAPPED o;
    if(size>4096)  size = 4096;

    /* added the void pointer cast, otherwise gcc will complain about */
    /* "warning: dereferencing type-punned pointer will break strict aliasing rules" */

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


#endif

void CSerial::ShowError()
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		0, // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	//printf("%Ls",lpMsgBuf);
//	MessageBox( NULL, (LPCTSTR)lpMsgBuf, L"Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

// virtual methods
void CSerial::OnConnect()
{
}
void CSerial::OnDisconnect()
{
}
void CSerial::OnData()
{
}
void CSerial::OnValid()
{
}
void CSerial::OnInvalid()
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
void CSerial::OnNewScan()
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