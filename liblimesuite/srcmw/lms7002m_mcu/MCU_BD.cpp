/**
@file	MCU_BD.cpp
@author	Lime Microsystems
@brief  Implementation of interacting with on board MCU
*/

#include "MCU_BD.h"
using namespace std;
#include <string>
#include "MCU_File.h"
#include <sstream>
#include <fstream>
#include "LMS64CProtocol.h"
#include <assert.h>
#include <thread>
#include <list>
#include "ErrorReporting.h"
#include "LMS7002M.h"

using namespace lime;

MCU_BD::MCU_BD()
{
    mLoadedProgramFilename = "";
    m_bLoadedDebug = 0;
    m_bLoadedProd = 0;
    stepsTotal = 0;
    stepsDone = 0;
    aborted = false;
    callback = nullptr;
    //ctor
    int i=0;
    m_serPort=NULL;
    //default value,
    //must be verified during program exploatation
    m_iLoopTries=20;
    byte_array_size = cMaxFWSize;
    // array initiallization
    for (i=0; i<=255; i++){
			m_SFR[i]=0x00;
			m_IRAM[i]=0x00;
	}
	for (i=0; i<byte_array_size; i++){
	    byte_array[i]=0x00;
    };
}

MCU_BD::~MCU_BD()
{
    //dtor
}

void MCU_BD::Initialize(IConnection* pSerPort, unsigned size)
{
    m_serPort = pSerPort;
    if (size > 0)
        byte_array_size = size;
}

/** @brief Read program code from file into memory
    @param inFileName source file path
    @param bin binary or hex file
    @return 0:success, -1:file not found
*/
int MCU_BD:: GetProgramCode(const char* inFileName, bool bin)
{
    unsigned char ch=0x00;
    bool find_byte=false;
    int i=0;

    if(!bin)
    {
        MCU_File	inFile(inFileName, "rb");
        if (inFile.FileOpened() == false)
            return -1;

        mLoadedProgramFilename = inFileName;
        try
        {
            inFile.ReadHex(byte_array_size-1);
        }
        catch (...)
        {
            return -1;
        }

        for (i=0; i<byte_array_size; i++)
        {
            find_byte=inFile.GetByte(i, ch);
            if (find_byte==true)
                byte_array[i]=ch;
            else
                byte_array[i]=0x00;
        };
    }
    else
    {
        char inByte = 0;
        fstream fin;
        fin.open(inFileName, ios::in | ios::binary);
        if (fin.good() == false)
            return -1;
        mLoadedProgramFilename = inFileName;
        memset(byte_array, 0, byte_array_size);
        for(int i=0; i<byte_array_size && !fin.eof(); ++i)
        {
            inByte = 0;
            fin.read(&inByte, 1);
            byte_array[i]=inByte;
        }
    }
    return 0;
}

void MCU_BD:: mSPI_write(
            unsigned short addr_reg,  // takes 16 bit address
            unsigned short data_reg)  // takes 16 bit value
{
    if(m_serPort == nullptr)
        return;
    uint32_t wrdata = addr_reg << 16 | data_reg;
    m_serPort->TransactSPI(0x10, &wrdata, nullptr, 1);
}


unsigned short MCU_BD:: mSPI_read(
            unsigned short addr_reg)  // takes 16 bit address
{// returns 16 bit value
    if(m_serPort == nullptr)
        return 0;
    uint32_t wrdata = addr_reg << 16;
    uint32_t rddata = 0;
    if(m_serPort->TransactSPI(0x10, &wrdata, &rddata, 1) != 0)
        return 0;
    return rddata & 0xFFFF;
}

int MCU_BD::WaitUntilWritten(){

	 // waits if WRITE_REQ (REG3[2]) flag is equal to '1'
	 // this means that  write operation is in progress
	unsigned short tempi=0x0000;
	int countDown=m_iLoopTries;  // Time out value
	tempi=mSPI_read(0x0003); // REG3 read

	while (( (tempi&0x0004) == 0x0004) && (countDown>0))
    {
		tempi=mSPI_read(0x0003); // REG3 read
		countDown--;
	}
	if (countDown==0)
        return -1; // an error occured, timer elapsed
	else
        return 0; // Finished regularly
	// pass if WRITE_REQ is '0'
}

int MCU_BD::ReadOneByte(unsigned char * data)
{
	unsigned short tempi=0x0000;
	int countDown=m_iLoopTries;

     // waits when READ_REQ (REG3[3]) flag is equal to '0'
	 // this means that there is nothing to read
	tempi=mSPI_read(0x0003); // REG3 read

    while (((tempi&0x0008)==0x0000) && (countDown>0))
    {
        // wait if READ_REQ is '0'
		tempi=mSPI_read(0x0003); // REG3 read
		countDown--;
	}

	if (countDown>0)
    { // Time out has not occured
		 tempi=mSPI_read(0x0005); // REG5 read
		  // return the read byte
		 (* data) = (unsigned char) (tempi);
	}
	else
        (* data) =0;
	 // return the zero, default value

	if (countDown==0)
        return -1; // an error occured
	else
        return 0; // finished regularly
}

int MCU_BD::One_byte_command(unsigned short data1, unsigned char * rdata1)
{
	unsigned char tempc=0x00;
	int retval=0;
	*rdata1=0x00; //default return value

	// sends the one byte command
	mSPI_write(0x8004, data1); //REG4 write
	retval=WaitUntilWritten();
	if (retval==-1) return -1;
	// error if operation executes too long

    // gets the one byte answer
	retval=ReadOneByte(&tempc);
    if (retval==-1) return -1;
	// error if operation takes too long

	*rdata1=tempc;
	return 0;
}


int MCU_BD::Three_byte_command(
	    unsigned char data1,unsigned char data2,unsigned char data3,
		unsigned char * rdata1,unsigned char * rdata2,unsigned char * rdata3){


		int retval=0;
        *rdata1=0x00;
		*rdata2=0x00;
		*rdata3=0x00;

		mSPI_write(0x8004, (unsigned short)(data1)); //REG4 write
		retval=WaitUntilWritten();
		if (retval==-1) return -1;

		mSPI_write(0x8004, (unsigned short)(data2)); //REG4 write
		retval=WaitUntilWritten();
		if (retval==-1) return -1;

		mSPI_write(0x8004, (unsigned short)(data3)); //REG4 write
		retval=WaitUntilWritten();
		if (retval==-1) return -1;

		retval= ReadOneByte(rdata1);
		if (retval==-1) return -1;

		retval= ReadOneByte(rdata2);
		if (retval==-1) return -1;

		retval= ReadOneByte(rdata3);
		if (retval==-1) return -1;

		return 0;
}

int MCU_BD::Change_MCUFrequency(unsigned char data) {

	unsigned char tempc1, tempc2, tempc3=0x00;
	int retval=0;
    // code 0x7E is for writing the SFR registers
	retval=Three_byte_command(0x7E, 0x8E, data, &tempc1, &tempc2, &tempc3);
	// PMSR register, address 0x8E
	return retval;
}

int MCU_BD::Read_IRAM()
{
	unsigned char tempc1, tempc2, tempc3=0x00;
	int i=0;
	int retval=0;

	//default
	//IRAM array initialization
	for (i=0; i<=255; i++)
		m_IRAM[i]=0x00;

    stepsTotal.store(256);
    stepsDone.store(0);
    aborted.store(false);
	for (i=0; i<=255; i++)
    {
        // code 0x78 is for reading the IRAM locations
		retval=Three_byte_command(0x78, ((unsigned char)(i)), 0x00,&tempc1, &tempc2, &tempc3);
		if (retval==0)
            m_IRAM[i]=tempc3;
		else
        {
            i=256; // error, stop
            aborted.store(true);
        }
        ++stepsDone;
#ifndef NDEBUG
        printf("MCU reading IRAM: %2i/256\r", stepsDone.load());
#endif
        Wait_CLK_Cycles(64);
	}
#ifndef NDEBUG
    printf("\nMCU reading IRAM finished\n");
#endif
	return retval;
}

int MCU_BD::Erase_IRAM()
{
	unsigned char tempc1, tempc2, tempc3=0x00;
	int retval=0;
	int i=0;

	//default ini.
	for (i=0; i<=255; i++)
			m_IRAM[i]=0x00;

    stepsTotal.store(256);
    stepsDone.store(0);
    aborted.store(false);
	for (i=0; i<=255; i++)
    {
			m_IRAM[i]=0x00;
            // code 0x7C is for writing the IRAM locations
			retval=Three_byte_command(0x7C, ((unsigned char)(i)), 0x00,&tempc1, &tempc2, &tempc3);
            if (retval == -1)
            {
                i = 256;
                aborted.store(true);
            }
            ++stepsDone;
#ifndef NDEBUG
            printf("MCU erasing IRAM: %2i/256\r", stepsDone.load());
#endif
	}
#ifndef NDEBUG
    printf("\nMCU erasing IRAM finished\n");
#endif
	return retval;
}

int MCU_BD::Read_SFR()
{
    int i=0;
	unsigned char tempc1, tempc2, tempc3=0x00;
	int retval=0;

    stepsTotal.store(48);
    stepsDone.store(0);
    aborted.store(false);

	//default m_SFR array initialization
	for (i=0; i<=255; i++)
			m_SFR[i]=0x00;

	// code 0x7A is for reading the SFR registers
	retval=Three_byte_command(0x7A, 0x80, 0x00, &tempc1, &tempc2, &tempc3); // P0
	if (retval==-1) return -1;
	m_SFR[0x80]=tempc3;

	retval=Three_byte_command(0x7A, 0x81, 0x00, &tempc1, &tempc2, &tempc3); // SP
	if (retval==-1) return -1;
	m_SFR[0x81]=tempc3;

	retval=Three_byte_command(0x7A, 0x82, 0x00, &tempc1, &tempc2, &tempc3); // DPL0
	if (retval==-1) return -1;
	m_SFR[0x82]=tempc3;

	retval=Three_byte_command(0x7A, 0x83, 0x00, &tempc1, &tempc2, &tempc3); // DPH0
	if (retval==-1) return -1;
	m_SFR[0x83]=tempc3;

	retval=Three_byte_command(0x7A, 0x84, 0x00, &tempc1, &tempc2, &tempc3); // DPL1
	if (retval==-1) return -1;
	m_SFR[0x84]=tempc3;

	retval=Three_byte_command(0x7A, 0x85, 0x00, &tempc1, &tempc2, &tempc3); // DPH1
	if (retval==-1) return -1;
	m_SFR[0x85]=tempc3;

    stepsDone.store(6);

	retval=Three_byte_command(0x7A, 0x86, 0x00, &tempc1, &tempc2, &tempc3); // DPS
	if (retval==-1) return -1;
	m_SFR[0x86]=tempc3;

	retval=Three_byte_command(0x7A, 0x87, 0x00, &tempc1, &tempc2, &tempc3); // PCON
	if (retval==-1) return -1;
	m_SFR[0x87]=tempc3;

	retval=Three_byte_command(0x7A, 0x88, 0x00, &tempc1, &tempc2, &tempc3); // TCON
	if (retval==-1) return -1;
	m_SFR[0x88]=tempc3;

	retval=Three_byte_command(0x7A, 0x89, 0x00, &tempc1, &tempc2, &tempc3); // TMOD
	if (retval==-1) return -1;
	m_SFR[0x89]=tempc3;

	retval=Three_byte_command(0x7A, 0x8A, 0x00, &tempc1, &tempc2, &tempc3); // TL0
	if (retval==-1) return -1;
	m_SFR[0x8A]=tempc3;

	retval=Three_byte_command(0x7A, 0x8B, 0x00, &tempc1, &tempc2, &tempc3); // TL1
	if (retval==-1) return -1;
	m_SFR[0x8B]=tempc3;

    stepsDone.store(12);

	retval=Three_byte_command(0x7A, 0x8C, 0x00, &tempc1, &tempc2, &tempc3); // TH0
	if (retval==-1) return -1;
	m_SFR[0x8C]=tempc3;

	retval=Three_byte_command(0x7A, 0x8D, 0x00, &tempc1, &tempc2, &tempc3); // TH1
	if (retval==-1) return -1;
    m_SFR[0x8D]=tempc3;

	retval=Three_byte_command(0x7A, 0x8E, 0x00, &tempc1, &tempc2, &tempc3); // PMSR
	if (retval==-1) return -1;
	m_SFR[0x8E]=tempc3;

	retval=Three_byte_command(0x7A, 0x90, 0x00, &tempc1, &tempc2, &tempc3); // P1
	if (retval==-1) return -1;
	m_SFR[0x90]=tempc3;

	retval=Three_byte_command(0x7A, 0x91, 0x00, &tempc1, &tempc2, &tempc3); // DIR1
	if (retval==-1) return -1;
	m_SFR[0x91]=tempc3;

	retval=Three_byte_command(0x7A, 0x98, 0x00, &tempc1, &tempc2, &tempc3); // SCON
	if (retval==-1) return -1;
	m_SFR[0x98]=tempc3;

    stepsDone.store(18);

	retval=Three_byte_command(0x7A, 0x99, 0x00, &tempc1, &tempc2, &tempc3); // SBUF
	if (retval==-1) return -1;
	m_SFR[0x99]=tempc3;

	retval=Three_byte_command(0x7A, 0xA0, 0x00, &tempc1, &tempc2, &tempc3); // P2
	if (retval==-1) return -1;
	m_SFR[0xA0]=tempc3;

	retval=Three_byte_command(0x7A, 0xA1, 0x00, &tempc1, &tempc2, &tempc3); // DIR2
	if (retval==-1) return -1;
	m_SFR[0xA1]=tempc3;

	retval=Three_byte_command(0x7A, 0xA2, 0x00, &tempc1, &tempc2, &tempc3); // DIR0
	if (retval==-1) return -1;
	m_SFR[0xA2]=tempc3;

	retval=Three_byte_command(0x7A, 0xA8, 0x00, &tempc1, &tempc2, &tempc3); // IEN0
	if (retval==-1) return -1;
	m_SFR[0xA8]=tempc3;

    stepsDone.store(24);

	retval=Three_byte_command(0x7A, 0xA9, 0x00, &tempc1, &tempc2, &tempc3); // IEN1
	if (retval==-1) return -1;
	m_SFR[0xA9]=tempc3;

	retval=Three_byte_command(0x7A, 0xB0, 0x00, &tempc1, &tempc2, &tempc3); // EECTRL
	if (retval==-1) return -1;
    m_SFR[0xB0]=tempc3;

	retval=Three_byte_command(0x7A, 0xB1, 0x00, &tempc1, &tempc2, &tempc3); // EEDATA
	if (retval==-1) return -1;
	m_SFR[0xB1]=tempc3;

	retval=Three_byte_command(0x7A, 0xB8, 0x00, &tempc1, &tempc2, &tempc3); // IP0
	if (retval==-1) return -1;
	m_SFR[0xB8]=tempc3;

	retval=Three_byte_command(0x7A, 0xB9, 0x00, &tempc1, &tempc2, &tempc3); // IP1
	if (retval==-1) return -1;
	m_SFR[0xB9]=tempc3;

	retval=Three_byte_command(0x7A, 0xBF, 0x00, &tempc1, &tempc2, &tempc3); // USR2
	if (retval==-1) return -1;
	m_SFR[0xBF]=tempc3;

    stepsDone.store(30);

	retval=Three_byte_command(0x7A, 0xC0, 0x00, &tempc1, &tempc2, &tempc3); // IRCON
	if (retval==-1) return -1;
	m_SFR[0xC0]=tempc3;

	retval=Three_byte_command(0x7A, 0xC8, 0x00, &tempc1, &tempc2, &tempc3); // T2CON
	if (retval==-1) return -1;
	m_SFR[0xC8]=tempc3;

	retval=Three_byte_command(0x7A, 0xCA, 0x00, &tempc1, &tempc2, &tempc3); // RCAP2L
	if (retval==-1) return -1;
	m_SFR[0xCA]=tempc3;

	retval=Three_byte_command(0x7A, 0xCB, 0x00, &tempc1, &tempc2, &tempc3); // RCAP2H
	if (retval==-1) return -1;
	m_SFR[0xCB]=tempc3;

	retval=Three_byte_command(0x7A, 0xCC, 0x00, &tempc1, &tempc2, &tempc3); // TL2
	if (retval==-1) return -1;
	m_SFR[0xCC]=tempc3;

	retval=Three_byte_command(0x7A, 0xCD, 0x00, &tempc1, &tempc2, &tempc3); // TH2
	if (retval==-1) return -1;
	m_SFR[0xCD]=tempc3;

    stepsDone.store(36);

	retval=Three_byte_command(0x7A, 0xD0, 0x00, &tempc1, &tempc2, &tempc3); // PSW
	if (retval==-1) return -1;
    m_SFR[0xD0]=tempc3;

	retval=Three_byte_command(0x7A, 0xE0, 0x00, &tempc1, &tempc2, &tempc3); // ACC
	if (retval==-1) return -1;
    m_SFR[0xE0]=tempc3;

	retval=Three_byte_command(0x7A, 0xF0, 0x00, &tempc1, &tempc2, &tempc3); // B
	if (retval==-1) return -1;
	m_SFR[0xF0]=tempc3;

	retval=Three_byte_command(0x7A, 0xEC, 0x00, &tempc1, &tempc2, &tempc3); // REG0
	if (retval==-1) return -1;
    m_SFR[0xEC]=tempc3;

	retval=Three_byte_command(0x7A, 0xED, 0x00, &tempc1, &tempc2, &tempc3); // REG1
	if (retval==-1) return -1;
	m_SFR[0xED]=tempc3;

	retval=Three_byte_command(0x7A, 0xEE, 0x00, &tempc1, &tempc2, &tempc3); // REG2
	if (retval==-1) return -1;
	m_SFR[0xEE]=tempc3;

    stepsDone.store(42);

	retval=Three_byte_command(0x7A, 0xEF, 0x00, &tempc1, &tempc2, &tempc3); // REG3
	if (retval==-1) return -1;
	m_SFR[0xEF]=tempc3;

	retval=Three_byte_command(0x7A, 0xF4, 0x00, &tempc1, &tempc2, &tempc3); // REG4
	if (retval==-1) return -1;
	m_SFR[0xF4]=tempc3;

	retval=Three_byte_command(0x7A, 0xF5, 0x00, &tempc1, &tempc2, &tempc3); // REG5
	if (retval==-1) return -1;
	m_SFR[0xF5]=tempc3;

	retval=Three_byte_command(0x7A, 0xF6, 0x00, &tempc1, &tempc2, &tempc3); // REG6
	if (retval==-1) return -1;
	m_SFR[0xF6]=tempc3;

	retval=Three_byte_command(0x7A, 0xF7, 0x00, &tempc1, &tempc2, &tempc3); // REG7
	if (retval==-1) return -1;
	m_SFR[0xF7]=tempc3;

	retval=Three_byte_command(0x7A, 0xFC, 0x00, &tempc1, &tempc2, &tempc3); // REG8
	if (retval==-1) return -1;
	m_SFR[0xFC]=tempc3;

	retval=Three_byte_command(0x7A, 0xFD, 0x00, &tempc1, &tempc2, &tempc3); // REG9
	if (retval==-1) return -1;
	m_SFR[0xFD]=tempc3;

    stepsDone.store(48);

	return 0;
}

void MCU_BD::Wait_CLK_Cycles(int delay)
{
	//// some delay
	int i=0;
	for (i=0;i<(delay/64);i++)
           mSPI_read(0x0003);
}

/** @brief Upload program code from memory into MCU
    @return 0:success, -1:failed
*/
int MCU_BD::Program_MCU(int m_iMode1, int m_iMode0)
{
    IConnection::MCU_PROG_MODE mode;
    switch(m_iMode1 << 1 | m_iMode0)
    {
    case 0: mode = IConnection::MCU_PROG_MODE::RESET; break;
    case 1: mode = IConnection::MCU_PROG_MODE::EEPROM_AND_SRAM; break;
    case 2: mode = IConnection::MCU_PROG_MODE::SRAM; break;
    case 3: mode = IConnection::MCU_PROG_MODE::BOOT_SRAM_FROM_EEPROM; break;
    default: mode = IConnection::MCU_PROG_MODE::RESET; break;
    }
    if(m_serPort)
        return m_serPort->ProgramMCU(byte_array, byte_array_size, mode, callback);
    else
        return ReportError(ENOLINK, "Device not connected");
}

int MCU_BD::Program_MCU(const uint8_t* binArray, const IConnection::MCU_PROG_MODE mode)
{
    if(m_serPort)
        return m_serPort->ProgramMCU(binArray, byte_array_size, mode, callback);
    else
        return ReportError(ENOLINK, "Device not connected");
}

void MCU_BD::Reset_MCU()
{
    unsigned short tempi=0x0000;  // was 0x0000
	mSPI_write(0x8002, tempi);
	tempi=0x0000;
	mSPI_write(0x8000, tempi);
}

int MCU_BD::RunProductionTest_MCU()
{
    string temps;
    unsigned short tempi = 0x0080;  // was 0x0000
    int m_iMode1_ = 0;
    int m_iMode0_ = 0;

    if (m_bLoadedProd == 0)
    {
        if (GetProgramCode("lms7suite_mcu/ptest.hex", false) != 0)
            return -1;
    }
    //MCU gets control over SPI switch
    mSPI_write(0x0006, 0x0001); //REG6 write

    // reset MCU
    tempi = 0x0080;
    mSPI_write(0x8002, tempi); // REG2
    tempi = 0x0000;
    mSPI_write(0x8000, tempi); // REG0

    if (m_bLoadedProd == 0)
    {
        //select programming mode "01" for SRAM and EEPROM
        m_iMode1_ = 0; m_iMode0_ = 1;
    }
    else
    {
        //boot from EEPROM
        m_iMode1_ = 1; m_iMode0_ = 1;
    }

    //upload hex file
    if (Program_MCU(m_iMode1_, m_iMode0_) != 0)
        return -1; //failed to program

    if (m_bLoadedProd == 0)
    {
        Wait_CLK_Cycles(256 * 100);  // for programming mode, prog.code has been already loaded into MCU
        m_iMode1_ = 0; m_iMode0_ = 1;
    }
    else
    {
        Wait_CLK_Cycles(256 * 400);
        // for booting from EEPROM mode, must wait for some longer delay, at least 8kB/(80kb/s)=0.1s
        m_iMode1_ = 1; m_iMode0_ = 1;
    }

    // global variable
    m_bLoadedProd = 1;  // the ptest.hex has been loaded
    m_bLoadedDebug = 0;

    //tempi = 0x0000;
    // EXT_INT2=1, external interrupt 2 is raised
    mSPI_write(0x8002, formREG2command(0, 0, 0, 1, m_iMode1_, m_iMode0_)); // EXT_INT2=1
    // here you can put any Delay function
    Wait_CLK_Cycles(256);
    // EXT_INT2=0, external interrupt 2 is pulled down
    mSPI_write(0x8002, formREG2command(0, 0, 0, 0, m_iMode1_, m_iMode0_)); // EXT_INT2=0

    // wait for some time MCU to execute the tests
    // the time is approximately 20ms
    // here you can put any Delay function
    Wait_CLK_Cycles(256 * 100);

    unsigned short retval = 0;
    retval = mSPI_read(1); //REG1 read

    // show the return value at the MCU Control Panel
    //int temps = wxString::Format("Result is: 0x%02X", retval);
    //ReadResult->SetLabel(temps);

    if (retval == 0x10)
    {
        tempi = 0x0055;
        mSPI_write(0x8000, tempi); // P0=0x55;
        // EXT_INT3=1, external interrupt 3 is raised
        mSPI_write(0x8002, formREG2command(0, 0, 1, 0, m_iMode1_, m_iMode0_)); // EXT_INT3=1
        // here you can put any Delay function
        Wait_CLK_Cycles(256);
        // EXT_INT3=0, external interrupt 3 is pulled down
        mSPI_write(0x8002, formREG2command(0, 0, 0, 0, m_iMode1_, m_iMode0_)); // EXT_INT3=0
        Wait_CLK_Cycles(256 * 5);
        retval = mSPI_read(1);  //REG1 read
        if (retval != 0x55)
            temps = "Ext. interrupt 3 test failed.";
        else
        {
            tempi = 0x00AA;
            mSPI_write(0x8000, tempi); // P0=0xAA;
            // EXT_INT4=1, external interrupt 4 is raised
            mSPI_write(0x8002, formREG2command(0, 1, 0, 0, m_iMode1_, m_iMode0_)); // EXT_INT4=1
            // here you can put any Delay function
            Wait_CLK_Cycles(256);
            // EXT_INT4=0, external interrupt 4 is pulled down
            mSPI_write(0x8002, formREG2command(0, 0, 0, 0, m_iMode1_, m_iMode0_)); // EXT_INT4=0
            Wait_CLK_Cycles(256 * 5);
            retval = mSPI_read(1);  //REG1 read
            if (retval != 0xAA)  temps = "Ext. interrupt 4 test failed.";
            else {
                tempi = 0x0055;
                mSPI_write(0x8000, tempi); // P0=0x55;
                // EXT_INT5=1, external interrupt 5 is raised
                mSPI_write(0x8002, formREG2command(1, 0, 0, 0, m_iMode1_, m_iMode0_)); // EXT_INT5=1
                // here you can put any Delay function
                Wait_CLK_Cycles(256);
                // EXT_INT5=0, external interrupt 5 is pulled down
                mSPI_write(0x8002, formREG2command(0, 0, 0, 0, m_iMode1_, m_iMode0_)); // EXT_INT5=0
                Wait_CLK_Cycles(256 * 5);
                retval = mSPI_read(1);  //REG1 read
                if (retval != 0x55)
                {
                    temps = "Ext. interrupt 5 test failed.";
                    return -1;
                }
                else
                {
                    temps = "Production test finished. MCU is OK.";
                    return 0;
                }
            }
        }
    }
    else
    {
        if ((retval & 0xF0) == 0x30)
        { // detected error code
            if ((retval & 0x0F) > 0)
            {
                char ctemp[64];
                sprintf(ctemp, "Test %i failed", (retval & 0x0F));
                temps = ctemp;
                return -1;
            }
            else
            {
                temps = "Test failed";
                return -1;
            }
        }
        else
        {
            // test too long. Failure.
            temps = "Test failed.";
            return -1;
        }
    }

    //Baseband gets back the control over SPI switch
    mSPI_write(0x0006, 0x0000); //REG6 write

    return 0;
}

void MCU_BD::RunTest_MCU(int m_iMode1, int m_iMode0, unsigned short test_code, int m_iDebug) {

	int i=0;
	int limit=0;
	unsigned short tempi=0x0000;
	unsigned short basei=0x0000;

	if  (test_code<=15) basei=(test_code<<4);
	else basei=0x0000;

	basei=basei&0xFFF0; // not necessery
	// 4 LSBs are zeros

	// variable basei contains test no. value at bit positions 7-4
	// used for driving the P0 input
	// P0 defines the test no.

	if ((test_code>7)||(test_code==0))
        limit=1;
	else
        limit=50;

	// tests 8 to 14 have short duration

	if (m_iDebug==1) return; // normal MCU operating mode required

	// EXT_INT2=1, external interrupt 2 is raised
	tempi=0x0000;  // changed
	int m_iExt2=1;

	if (m_iExt2==1)  tempi=tempi|0x0004;
	if (m_iMode1==1) tempi=tempi|0x0002;
	if (m_iMode0==1) tempi=tempi|0x0001;

	// tempi variable is driving the mspi_REG2

	mSPI_write(0x8002, tempi); // REG2 write

	// generating waveform
	for (i=0; i<=limit; i++)
    {
		tempi=basei|0x000C;
		mSPI_write(0x8000, tempi);
		// REG0 write
		Wait_CLK_Cycles(256);
        tempi=basei|0x000D;
		mSPI_write(0x8000, tempi);
		// REG0 write  - P0(0) set
		Wait_CLK_Cycles(256);
		tempi=basei|0x000C;
		mSPI_write(0x8000, tempi);
		// REG0 write
		Wait_CLK_Cycles(256);
		tempi=basei|0x000E;
		mSPI_write(0x8000, tempi);
		// REG0 write - PO(1) set
		Wait_CLK_Cycles(256);

		if (i==0) {
            // EXT_INT2=0
            // external interrupt 2 is pulled down
			tempi=0x0000; // changed
			m_iExt2=0;
			if (m_iExt2==1)  tempi=tempi|0x0004;
			if (m_iMode1==1) tempi=tempi|0x0002;
			if (m_iMode0==1) tempi=tempi|0x0001;
			mSPI_write(0x8002, tempi);
            // REG2 write
		}
	}
}


void MCU_BD::RunFabTest_MCU(int m_iMode1, int m_iMode0, int m_iDebug) {

	unsigned short tempi=0x0000;

  	if (m_iDebug==1) return; // normal MCU operating mode required

	// EXT_INT2=1, external interrupt 2 is raised
	tempi=0x0000;  // changed
	int m_iExt2=1;
	if (m_iExt2==1)  tempi=tempi|0x0004;
	if (m_iMode1==1) tempi=tempi|0x0002;
	if (m_iMode0==1) tempi=tempi|0x0001;
	mSPI_write(0x8002, tempi); // REG2 write

	Wait_CLK_Cycles(256);

	// EXT_INT2=0, external interrupt 2 is pulled down
	tempi=0x0000; // changed
    m_iExt2=0;
	if (m_iExt2==1)  tempi=tempi|0x0004;
	if (m_iMode1==1) tempi=tempi|0x0002;
	if (m_iMode0==1) tempi=tempi|0x0001;
	mSPI_write(0x8002, tempi);

	Wait_CLK_Cycles(256);

}

void MCU_BD::DebugModeSet_MCU(int m_iMode1, int m_iMode0)
{
        unsigned short tempi=0x00C0;
        // bit DEBUG is set
		int m_iExt2=0;
		if (m_iExt2==1)  tempi=tempi|0x0004;
		if (m_iMode1==1) tempi=tempi|0x0002;
		if (m_iMode0==1) tempi=tempi|0x0001;

		// Select debug mode
		mSPI_write(0x8002, tempi);
		// REG2 write
}

 void MCU_BD::DebugModeExit_MCU(int m_iMode1, int m_iMode0)
{

        unsigned short tempi=0x0000; // bit DEBUG is zero
		int m_iExt2=0;

		if (m_iExt2==1)  tempi=tempi|0x0004;
		if (m_iMode1==1) tempi=tempi|0x0002;
		if (m_iMode0==1) tempi=tempi|0x0001;
		// To run mode
		mSPI_write(0x8002, tempi);  // REG2 write
}

int MCU_BD::ResetPC_MCU()
{
     unsigned char tempc1=0x00;
     int retval=0;
     retval=One_byte_command(0x70, &tempc1);
     return retval;
}

int MCU_BD::RunInstr_MCU(unsigned short * pPCVAL)
{
    unsigned char tempc1, tempc2, tempc3=0x00;
    int retval=0;
    retval=Three_byte_command(0x74, 0x00, 0x00, &tempc1, &tempc2, &tempc3);
	if (retval==-1) (*pPCVAL)=0;
	else (*pPCVAL)=tempc2*256+tempc3;
	return retval;
}

void MCU_BD::Log(const char* msg)
{
    printf("%s", msg);
}

/** @brief Returns information about programming or reading data progress
*/
MCU_BD::ProgressInfo MCU_BD::GetProgressInfo() const
{
    ProgressInfo info;
    info.stepsDone = stepsDone.load();
    info.stepsTotal = stepsTotal.load();
    info.aborted = aborted.load();
    return info;
}

unsigned int MCU_BD::formREG2command(int m_iExt5, int m_iExt4, int m_iExt3, int m_iExt2, int m_iMode1, int m_iMode0) {
    unsigned int tempi = 0x0000;
    if (m_iExt5 == 1)  tempi = tempi | 0x0020;
    if (m_iExt4 == 1)  tempi = tempi | 0x0010;
    if (m_iExt3 == 1)  tempi = tempi | 0x0008;
    if (m_iExt2 == 1)  tempi = tempi | 0x0004;
    if (m_iMode1 == 1) tempi = tempi | 0x0002;
    if (m_iMode0 == 1) tempi = tempi | 0x0001;
    return(tempi);
}

std::string MCU_BD::GetProgramFilename() const
{
    return mLoadedProgramFilename;
}

/** @brief Starts algorithm in MCU
*/
void MCU_BD::RunProcedure(uint8_t id)
{
    mSPI_write(0x0006, id != 0);
    mSPI_write(0x0000, id);
    uint8_t x0002reg = mSPI_read(0x0002);
    const uint8_t interupt6 = 0x08;
    mSPI_write(0x0002, x0002reg | interupt6);
    mSPI_write(0x0002, x0002reg & ~interupt6);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
}


/** @brief Waits for MCU to finish executing program
@return 0 success, 255 idle, 244 running, else algorithm status
*/
int MCU_BD::WaitForMCU(uint32_t timeout_ms)
{
    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = t1;
    unsigned short value = 0;
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    do {
        value = mSPI_read(0x0001) & 0xFF;
        if (value != 0xFF) //working
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        t2 = std::chrono::high_resolution_clock::now();
    }while (std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() < timeout_ms);
    mSPI_write(0x0006, 0); //return SPI control to PC
    //if((value & 0x7f) != 0)
        std::printf("MCU algorithm time: %li ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    return value & 0x7F;
}

void MCU_BD::SetParameter(MCU_Parameter param, float value)
{
    uint8_t inputRegs[3];
    value /= 1e6;
    inputRegs[0] = (uint8_t)value; //frequency integer part

    uint16_t fracPart = value * 1000.0 - inputRegs[0]*1000.0;
    inputRegs[1] = (fracPart >> 8) & 0xFF;
    inputRegs[2] = fracPart & 0xFF;
    const uint8_t x0002reg = mSPI_read(0x0002);
    const uint8_t interupt7 = 0x04;
    for(uint8_t i = 0; i < 3; ++i)
    {
        mSPI_write(0, inputRegs[2-i]);
        mSPI_write(0x0002, x0002reg | interupt7);
        mSPI_write(0x0002, x0002reg & ~interupt7);
        this_thread::sleep_for(chrono::microseconds(5));
    }
    if(param==MCU_REF_CLK)
        RunProcedure(4);
    if(param == MCU_BW)
        RunProcedure(3);
}

/** @brief Switches MCU into debug mode, MCU program execution is halted
    @param mode MCU memory initialization mode
    @return Operation status
*/
MCU_BD::OperationStatus MCU_BD::SetDebugMode(bool enabled, IConnection::MCU_PROG_MODE mode)
{
    uint8_t regValue = 0;
    switch (mode)
    {
    case IConnection::MCU_PROG_MODE::RESET:
        break;
    case IConnection::MCU_PROG_MODE::EEPROM_AND_SRAM:
        regValue |= 0x01; break;
    case IConnection::MCU_PROG_MODE::SRAM:
        regValue |= 0x02; break;
    case IConnection::MCU_PROG_MODE::BOOT_SRAM_FROM_EEPROM:
        regValue |= 0x03; break;
    }
    if (enabled)
        regValue |= 0xC0;
    mSPI_write(0x8002, regValue);
    return SUCCESS;
}

MCU_BD::OperationStatus MCU_BD::readIRAM(const uint8_t *addr, uint8_t* values, const uint8_t count)
{
    uint8_t cmd = 0x78; //
    int retval;
    for (int i = 0; i < count; ++i)
    {
        mSPI_write(0x8004, cmd); //REG4 write cmd
        retval = WaitUntilWritten();
        if (retval == -1) return FAILURE;

        mSPI_write(0x8004, addr[i]); //REG4 write IRAM address
        retval = WaitUntilWritten();
        if (retval == -1) return FAILURE;

        mSPI_write(0x8004, 0); //REG4 nop
        retval = WaitUntilWritten();
        if (retval == -1) return FAILURE;

        uint8_t result = 0;
        retval = ReadOneByte(&result);
        if (retval == -1) return FAILURE;

        retval = ReadOneByte(&result);
        if (retval == -1) return FAILURE;

        retval = ReadOneByte(&result);
        if (retval == -1) return FAILURE;
        values[i] = result;
    }
    return SUCCESS;
}

MCU_BD::OperationStatus MCU_BD::writeIRAM(const uint8_t *addr, const uint8_t* values, const uint8_t count)
{
    return FAILURE;
}

uint8_t MCU_BD::ReadMCUProgramID()
{
    RunProcedure(255);
    auto statusMcu = WaitForMCU(10);
    return statusMcu & 0x7F;
}
