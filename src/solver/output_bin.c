#include <math.h>

#include "headers.h"
#include "version.h"
#include "output.h"


static void output_openOutFile(void);
static void output_saveID(char* id, FILE* file);

enum InputDataType {INPUT_TYPE_CODE, INPUT_AREA, INPUT_INVERT, INPUT_MAX_DEPTH,
                    INPUT_OFFSET, INPUT_LENGTH};


//=============================================================================

void  output_checkFileSize()
//
//  Input:   none
//  Output:  none
//  Purpose: checks if the size of the binary output file will be too big
//           to access using an integer file pointer variable.
//
{
    if ( RptFlags.subcatchments != NONE ||
         RptFlags.nodes != NONE ||
         RptFlags.links != NONE )
    {
        if ( (double)OutputStartPos + (double)BytesPerPeriod * TotalDuration
             / 1000.0 / (double)ReportStep >= (double)MAXFILESIZE )
        {
            report_writeErrorMsg(ERR_FILE_SIZE, "");
        }
    }
}


void output_saveID(char* id, FILE* file)
//
//  Input:   id = name of an object
//           file = ptr. to binary output file
//  Output:  none
//  Purpose: writes an object's name to the binary output file.
//
{
    INT4 n = strlen(id);
    fwrite(&n, sizeof(INT4), 1, file);
    fwrite(id, sizeof(char), n, file);
}


//=============================================================================

void output_openOutFile()
//
//  Input:   none
//  Output:  none
//  Purpose: opens a project's binary output file.
//
{
    // --- close output file if already opened
    if (Fout.file != NULL) fclose(Fout.file); 

    // --- else if file name supplied then set file mode to SAVE
    else if (strlen(Fout.name) != 0) Fout.mode = SAVE_FILE;

    // --- otherwise set file mode to SCRATCH & generate a name
    else
    {
        Fout.mode = SCRATCH_FILE;
        getTempFileName(Fout.name);
    }

    // --- try to open the file
    if ( (Fout.file = fopen(Fout.name, "w+b")) == NULL)
    {
        writecon(FMT14);
        ErrorCode = ERR_OUT_FILE;
    }
}


int output_out_init(){
    int   j;
    int   m;
    INT4  k;
    REAL4 x;
    REAL8 z;
    // --- open binary output file
    output_openOutFile();
    
    BytesPerPeriod = sizeof(REAL8)
        + NumSubcatch * NumSubcatchVars * sizeof(REAL4)
        + NumNodes * NumNodeVars * sizeof(REAL4)
        + NumLinks * NumLinkVars * sizeof(REAL4)
        + MAX_SYS_RESULTS * sizeof(REAL4);

    fseek(Fout.file, 0, SEEK_SET);
    k = MAGICNUMBER;
    fwrite(&k, sizeof(INT4), 1, Fout.file);   // Magic number
    k = get_version_legacy();
    fwrite(&k, sizeof(INT4), 1, Fout.file);   // Version number
    k = FlowUnits;
    fwrite(&k, sizeof(INT4), 1, Fout.file);   // Flow units
    k = NumSubcatch;
    fwrite(&k, sizeof(INT4), 1, Fout.file);   // # subcatchments
    k = NumNodes;
    fwrite(&k, sizeof(INT4), 1, Fout.file);   // # nodes
    k = NumLinks;
    fwrite(&k, sizeof(INT4), 1, Fout.file);   // # links
    k = NumPolluts;
    fwrite(&k, sizeof(INT4), 1, Fout.file);   // # pollutants

    // --- save ID names of subcatchments, nodes, links, & pollutants 
    IDStartPos = ftell(Fout.file);
    for (j=0; j<Nobjects[SUBCATCH]; j++)
    {
        if ( Subcatch[j].rptFlag ) output_saveID(Subcatch[j].ID, Fout.file);
    }
    for (j=0; j<Nobjects[NODE];     j++)
    {
        if ( Node[j].rptFlag ) output_saveID(Node[j].ID, Fout.file);
    }
    for (j=0; j<Nobjects[LINK];     j++)
    {
        if ( Link[j].rptFlag ) output_saveID(Link[j].ID, Fout.file);
    }
    for (j=0; j<NumPolluts; j++) output_saveID(Pollut[j].ID, Fout.file);

    // --- save codes of pollutant concentration units
    for (j=0; j<NumPolluts; j++)
    {
        k = Pollut[j].units;
        fwrite(&k, sizeof(INT4), 1, Fout.file);
    }

    InputStartPos = ftell(Fout.file);

    // --- save subcatchment area
    k = 1;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_AREA;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    for (j=0; j<Nobjects[SUBCATCH]; j++)
    {
         if ( !Subcatch[j].rptFlag ) continue;
         SubcatchResults[0] = (REAL4)(Subcatch[j].area * UCF(LANDAREA));
         fwrite(&SubcatchResults[0], sizeof(REAL4), 1, Fout.file);
    }

    // --- save node type, invert, & max. depth
    k = 3;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_TYPE_CODE;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_INVERT;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_MAX_DEPTH;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    for (j=0; j<Nobjects[NODE]; j++)
    {
        if ( !Node[j].rptFlag ) continue;
        k = Node[j].type;
        NodeResults[0] = (REAL4)(Node[j].invertElev * UCF(LENGTH));
        NodeResults[1] = (REAL4)(Node[j].fullDepth * UCF(LENGTH));
        fwrite(&k, sizeof(INT4), 1, Fout.file);
        fwrite(NodeResults, sizeof(REAL4), 2, Fout.file);
    }

    // --- save link type, offsets, max. depth, & length
    k = 5;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_TYPE_CODE;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_OFFSET;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_OFFSET;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_MAX_DEPTH;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = INPUT_LENGTH;
    fwrite(&k, sizeof(INT4), 1, Fout.file);

    for (j=0; j<Nobjects[LINK]; j++)
    {
        if ( !Link[j].rptFlag ) continue;
        k = Link[j].type;
        if ( k == PUMP )
        {
            for (m=0; m<4; m++) LinkResults[m] = 0.0f;
        }
        else
        {
            LinkResults[0] = (REAL4)(Link[j].offset1 * UCF(LENGTH));
            LinkResults[1] = (REAL4)(Link[j].offset2 * UCF(LENGTH));
            if ( Link[j].direction < 0 )
            {
                x = LinkResults[0];
                LinkResults[0] = LinkResults[1];
                LinkResults[1] = x;
            }
            if ( k == OUTLET ) LinkResults[2] = 0.0f;
            else LinkResults[2] = (REAL4)(Link[j].xsect.yFull * UCF(LENGTH));
            if ( k == CONDUIT )
            {
                m = Link[j].subIndex;
                LinkResults[3] = (REAL4)(Conduit[m].length * UCF(LENGTH));
            }
            else LinkResults[3] = 0.0f;
        }
        fwrite(&k, sizeof(INT4), 1, Fout.file);
        fwrite(LinkResults, sizeof(REAL4), 4, Fout.file);
    }

    // --- save number & codes of subcatchment result variables
    k = NumSubcatchVars;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_RAINFALL;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_SNOWDEPTH;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_EVAP;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_INFIL;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_RUNOFF;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_GW_FLOW;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_GW_ELEV;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = SUBCATCH_SOIL_MOIST;
    fwrite(&k, sizeof(INT4), 1, Fout.file);

    for (j=0; j<NumPolluts; j++) 
    {
        k = SUBCATCH_WASHOFF + j;
        fwrite(&k, sizeof(INT4), 1, Fout.file);
    }

    // --- save number & codes of node result variables
    k = NumNodeVars;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = NODE_DEPTH;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = NODE_HEAD;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = NODE_VOLUME;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = NODE_LATFLOW;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = NODE_INFLOW;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = NODE_OVERFLOW;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    for (j=0; j<NumPolluts; j++)
    {
        k = NODE_QUAL + j;
        fwrite(&k, sizeof(INT4), 1, Fout.file);
    }

    // --- save number & codes of link result variables
    k = NumLinkVars;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = LINK_FLOW;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = LINK_DEPTH;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = LINK_VELOCITY;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = LINK_VOLUME;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = LINK_CAPACITY;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    for (j=0; j<NumPolluts; j++)
    {
        k = LINK_QUAL + j;
        fwrite(&k, sizeof(INT4), 1, Fout.file);
    }

    // --- save number & codes of system result variables
    k = MAX_SYS_RESULTS;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    for (k=0; k<MAX_SYS_RESULTS; k++) fwrite(&k, sizeof(INT4), 1, Fout.file);

    // --- save starting report date & report step
    //     (if reporting start date > simulation start date then
    //      make saved starting report date one reporting period
    //      prior to the date of the first reported result)
    z = (double)ReportStep/86400.0;
    if ( StartDateTime + z > ReportStart ) z = StartDateTime;
    else
    {
        z = floor((ReportStart - StartDateTime)/z) - 1.0;
        z = StartDateTime + z*(double)ReportStep/86400.0;
    }
    fwrite(&z, sizeof(REAL8), 1, Fout.file);
    k = ReportStep;
    if ( fwrite(&k, sizeof(INT4), 1, Fout.file) < 1)
    {
        report_writeErrorMsg(ERR_OUT_WRITE, "");
        return ErrorCode;
    }
    OutputStartPos = ftell(Fout.file);
    if ( Fout.mode == SCRATCH_FILE ) output_checkFileSize();
    return ErrorCode;
}

int bin_saveSubcatchResults(char *ID,DateTime reportDate,TFile Fout){
    fwrite(SubcatchResults, sizeof(REAL4), NumSubcatchVars, Fout.file);
    return 0;
}

int bin_saveNodeResults(char *ID,DateTime reportDate,TFile Fout){
    fwrite(NodeResults, sizeof(REAL4), NumNodeVars, Fout.file);
    return 0;
}

int bin_saveLinkResults(char *ID,DateTime reportDate,TFile Fout){
    fwrite(LinkResults, sizeof(REAL4), NumLinkVars, Fout.file);
    return 0;
}

int bin_saveSysResults(DateTime reportDate,TFile Fout){
    fwrite(SysResults, sizeof(REAL4), MAX_SYS_RESULTS, Fout.file);
    return 0;
}


void bin_saveDate(DateTime reportDate,TFile Fout){
    REAL8 date;
    date = reportDate;

    fwrite(&date, sizeof(REAL8), 1, Fout.file);

}

void bin_output_end()
//
//  Input:   none
//  Output:  none
//  Purpose: writes closing records to binary file.
//
{
    INT4 k;
    fwrite(&IDStartPos, sizeof(INT4), 1, Fout.file);
    fwrite(&InputStartPos, sizeof(INT4), 1, Fout.file);
    fwrite(&OutputStartPos, sizeof(INT4), 1, Fout.file);
    k = Nperiods;
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = (INT4)error_getCode(ErrorCode);
    fwrite(&k, sizeof(INT4), 1, Fout.file);
    k = MAGICNUMBER;
    if (fwrite(&k, sizeof(INT4), 1, Fout.file) < 1)
    {
        report_writeErrorMsg(ERR_OUT_WRITE, "");
    }
}

void bin_output_close(){
    if ( Fout.file != NULL )
    {
        fclose(Fout.file);
        if ( Fout.mode == SCRATCH_FILE ) remove(Fout.name);
    }
}

void bin_readSubcatchResults(int period, int index){
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    bytePos += sizeof(REAL8) + index*NumSubcatchVars*sizeof(REAL4);
    fseek(Fout.file, bytePos, SEEK_SET);
    fread(SubcatchResults, sizeof(REAL4), NumSubcatchVars, Fout.file);
}


//=============================================================================

void output_readDateTime(int period, DateTime* days)
//
//  Input:   period = index of reporting time period
//  Output:  days = date/time value
//  Purpose: retrieves the date/time for a specific reporting period
//           from the binary output file.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    fseek(Fout.file, bytePos, SEEK_SET);
    *days = NO_DATE;
    fread(days, sizeof(REAL8), 1, Fout.file);
}

//=============================================================================

void output_readSubcatchResults(int period, int index)
//
//  Input:   period = index of reporting time period
//           index = subcatchment index
//  Output:  none
//  Purpose: reads computed results for a subcatchment at a specific time
//           period.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    bytePos += sizeof(REAL8) + index*NumSubcatchVars*sizeof(REAL4);
    fseek(Fout.file, bytePos, SEEK_SET);
    fread(SubcatchResults, sizeof(REAL4), NumSubcatchVars, Fout.file);
}

//=============================================================================

void output_readNodeResults(int period, int index)
//
//  Input:   period = index of reporting time period
//           index = node index
//  Output:  none
//  Purpose: reads computed results for a node at a specific time period.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    bytePos += sizeof(REAL8) + NumSubcatch*NumSubcatchVars*sizeof(REAL4);
    bytePos += index*NumNodeVars*sizeof(REAL4);
    fseek(Fout.file, bytePos, SEEK_SET);
    fread(NodeResults, sizeof(REAL4), NumNodeVars, Fout.file);
}

//=============================================================================

void output_readLinkResults(int period, int index)
//
//  Input:   period = index of reporting time period
//           index = link index
//  Output:  none
//  Purpose: reads computed results for a link at a specific time period.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    bytePos += sizeof(REAL8) + NumSubcatch*NumSubcatchVars*sizeof(REAL4);
    bytePos += NumNodes*NumNodeVars*sizeof(REAL4);
    bytePos += index*NumLinkVars*sizeof(REAL4);
    fseek(Fout.file, bytePos, SEEK_SET);
    fread(LinkResults, sizeof(REAL4), NumLinkVars, Fout.file);
    fread(SysResults, sizeof(REAL4), MAX_SYS_RESULTS, Fout.file);
}