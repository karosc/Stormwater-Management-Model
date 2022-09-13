//-----------------------------------------------------------------------------
//   output.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14  (Build 5.1.001)
//             03/19/15  (Build 5.1.008)
//             08/05/15  (Build 5.1.010)
//             05/10/18  (Build 5.1.013)
//             03/01/20  (Build 5.1.014)
//   Author:   L. Rossman (EPA)
//
//   Binary output file access functions.
//
//   Build 5.1.008:
//   - Possible divide by zero for reported system wide variables avoided.
//   - Updating of maximum node depth at reporting times added.
//
//   Build 5.1.010:
//   - Potentional ET added to list of system-wide variables saved to file.
//
//   Build 5.1.013:
//   - Names NsubcatchVars, NnodeVars & NlinkVars replaced with
//     NumSubcatchVars, NumNodeVars & NumLinkVars 
//   - Support added for saving average node & link routing results to
//     binary file in each reporting period.
//
//   Build 5.1.014:
//   - Incorrect loop limit fixed in function output_saveAvgResults.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "headers.h"
#include "version.h"
#include "output.h"


enum InputDataType {INPUT_TYPE_CODE, INPUT_AREA, INPUT_INVERT, INPUT_MAX_DEPTH,
                    INPUT_OFFSET, INPUT_LENGTH};

typedef struct                                                                 //(5.1.013)
{                                                                              //
    REAL4* xAvg;                                                               //
}   TAvgResults;                                                               //

//-----------------------------------------------------------------------------
//  Shared variables    
//-----------------------------------------------------------------------------
INT4      IDStartPos;           // starting file position of ID names
INT4      InputStartPos;        // starting file position of input data
INT4      OutputStartPos;       // starting file position of output data
INT4      BytesPerPeriod;       // bytes saved per simulation time period
INT4      NumSubcatchVars;      // number of subcatchment output variables
INT4      NumNodeVars;          // number of node output variables
INT4      NumLinkVars;          // number of link output variables
INT4      NumSubcatch;          // number of subcatchments reported on
INT4      NumNodes;             // number of nodes reported on
INT4      NumLinks;             // number of links reported on
INT4      NumPolluts;           // number of pollutants reported on
REAL4     SysResults[MAX_SYS_RESULTS];    // values of system output vars.

static int outfile_type;    // type of outfile used (sqlite vs binary)

static TAvgResults* AvgLinkResults;                                            //(5.1.013)
static TAvgResults* AvgNodeResults;                                            //
static int          Nsteps;                                                    //

//-----------------------------------------------------------------------------
//  Exportable variables (shared with report.c)
//-----------------------------------------------------------------------------
REAL4*           SubcatchResults;
REAL4*           NodeResults;
REAL4*           LinkResults;


//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------

static void output_saveSubcatchResults(double reportTime, TFile Fout);
static void output_saveNodeResults(double reportTime, TFile Fout);
static void output_saveLinkResults(double reportTime, TFile Fout);

static int  output_openAvgResults(void);                                       //(5.1.013)
static void output_closeAvgResults(void);                                      //
static void output_initAvgResults(void);                                       //
static void output_saveAvgResults(double reportTime,  TFile Fout);                                 //


static int output_iface( TFile Fout);                                          //determine the format of outfile based on file extension

//-----------------------------------------------------------------------------
// function pointers that will point to appropriate output interface functions
//-----------------------------------------------------------------------------
int (*OutIface_saveNodeResults)(char*,DateTime ,TFile);
int (*OutIface_saveLinkResults)(char*,DateTime ,TFile);
int (*OutIface_saveSubcatchResults)(char*,DateTime ,TFile);
int (*OutIface_saveSysResults)(DateTime ,TFile);
int (*OutIface_init)(void);
void (*OutIface_output_end)(void);
void (*OutIface_output_close)(void);

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  output_open                   (called by swmm_start in swmm5.c)
//  output_end                    (called by swmm_end in swmm5.c)
//  output_close                  (called by swmm_close in swmm5.c)
//  output_updateAvgResults       (called by swmm_step in swmm5.c)             //(5.1.013)
//  output_saveResults            (called by swmm_step in swmm5.c)
//  output_checkFileSize          (called by swmm_report)
//  output_readDateTime           (called by routines in report.c)
//  output_readSubcatchResults    (called by report_Subcatchments)
//  output_readNodeResults        (called by report_Nodes)
//  output_readLinkResults        (called by report_Links)

const char *get_filename_ext(const char *filename) 
//
//  Input:   filename = file name string
//  Output:  extension of file name
//  Purpose: To extract the extension from a file name
//
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}


int output_iface(TFile Fout)
//
//  Input:   Fout = output file struct
//  Output:  outfile_type  integer
//  Purpose: To determine the appropriate output file 
//           format to use based on the file name extension
//
{

    if (strcmp(get_filename_ext(Fout.name),"db")==0) return 1;

    return 0;
}

int output_open()
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: opens and writes basic project data to output file.
//
{
    
    int   j;
    
    outfile_type = output_iface(Fout);


    // switch block that aliases output file writing functions to interface function pointers
    // used throughout this file. 
    switch(outfile_type){
        case 0:
            OutIface_saveNodeResults = bin_saveNodeResults;
            OutIface_saveLinkResults = bin_saveLinkResults;
            OutIface_saveSubcatchResults = bin_saveSubcatchResults;
            OutIface_saveSysResults = bin_saveSysResults;
            OutIface_init = output_out_init;
            OutIface_output_end = bin_output_end;
            OutIface_output_close = bin_output_close;
            break;
        
        case 1:
            OutIface_saveNodeResults = sql_saveNodeResults;
            OutIface_saveLinkResults = sql_saveLinkResults;
            OutIface_saveSubcatchResults = sql_saveSubcatchResults;
            OutIface_saveSysResults = sql_saveSysResults;
            OutIface_init = output_sql_init;
            OutIface_output_end = sql_output_end;
            OutIface_output_close = sql_output_close;
            
    }

    if ( ErrorCode ) return ErrorCode;

    // --- ignore pollutants if no water quality analsis performed
    if ( IgnoreQuality ) NumPolluts = 0;
    else NumPolluts = Nobjects[POLLUT];

    // --- subcatchment results consist of Rainfall, Snowdepth, Evap, 
    //     Infil, Runoff, GW Flow, GW Elev, GW Sat, and Washoff
    NumSubcatchVars = MAX_SUBCATCH_RESULTS - 1 + NumPolluts;

    // --- node results consist of Depth, Head, Volume, Lateral Inflow,
    //     Total Inflow, Overflow and Quality
    NumNodeVars = MAX_NODE_RESULTS - 1 + NumPolluts;

    // --- link results consist of Depth, Flow, Velocity, Volume,              //(5.1.013)
    //     Capacity and Quality
    NumLinkVars = MAX_LINK_RESULTS - 1 + NumPolluts;

    // --- get number of objects reported on
    NumSubcatch = 0;
    NumNodes = 0;
    NumLinks = 0;
    for (j=0; j<Nobjects[SUBCATCH]; j++) if (Subcatch[j].rptFlag) NumSubcatch++;
    for (j=0; j<Nobjects[NODE]; j++) if (Node[j].rptFlag) NumNodes++;
    for (j=0; j<Nobjects[LINK]; j++) if (Link[j].rptFlag) NumLinks++;

    
    Nperiods = 0;

    SubcatchResults = NULL;
    NodeResults = NULL;
    LinkResults = NULL;
    SubcatchResults = (REAL4 *) calloc(NumSubcatchVars, sizeof(REAL4));
    NodeResults = (REAL4 *) calloc(NumNodeVars, sizeof(REAL4));
    LinkResults = (REAL4 *) calloc(NumLinkVars, sizeof(REAL4));
    if ( !SubcatchResults || !NodeResults || !LinkResults )
    {
        report_writeErrorMsg(ERR_MEMORY, "");
        return ErrorCode;
    }

    // --- allocate memory to store average node & link results per period     //(5.1.013)
    AvgNodeResults = NULL;                                                     //
    AvgLinkResults = NULL;                                                     //
    if ( RptFlags.averages && !output_openAvgResults() )                       //
    {                                                                          //
        report_writeErrorMsg(ERR_MEMORY, "");                                  //
        return ErrorCode;                                                      //
    }                                                                          //

    ErrorCode = OutIface_init();
    return ErrorCode;

}



//=============================================================================

void output_saveResults(double reportTime)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//  Output:  none
//  Purpose: writes computed results for current report time to binary file.
//
{
    int i;
    extern TRoutingTotals StepFlowTotals;  // defined in massbal.c             //(5.1.013)
    DateTime reportDate = getDateTime(reportTime);
    // REAL8 date;

    // --- initialize system-wide results
    if ( reportDate < ReportStart ) return;
    for (i=0; i<MAX_SYS_RESULTS; i++) SysResults[i] = 0.0f;

    // --- save date corresponding to this elapsed reporting time
    if (outfile_type==0) bin_saveDate(reportDate,Fout);
    // --- save subcatchment results
    if (Nobjects[SUBCATCH] > 0)
        output_saveSubcatchResults(reportTime, Fout);

    // --- save average routing results over reporting period if called for    //(5.1.013)
    if ( RptFlags.averages ) output_saveAvgResults(reportTime,Fout);                 //

    // --- otherwise save interpolated point routing results                   //(5.1.013)
    else                                                                       //
    {
        if (Nobjects[NODE] > 0)
            output_saveNodeResults(reportTime, Fout);
        if (Nobjects[LINK] > 0)
            output_saveLinkResults(reportTime, Fout);
    }

    // --- update & save system-wide flows 
    SysResults[SYS_FLOODING] = (REAL4)(StepFlowTotals.flooding * UCF(FLOW));
    SysResults[SYS_OUTFLOW] = (REAL4)(StepFlowTotals.outflow * UCF(FLOW));
    SysResults[SYS_DWFLOW] = (REAL4)(StepFlowTotals.dwInflow * UCF(FLOW));
    SysResults[SYS_GWFLOW] = (REAL4)(StepFlowTotals.gwInflow * UCF(FLOW));
    SysResults[SYS_IIFLOW] = (REAL4)(StepFlowTotals.iiInflow * UCF(FLOW));
    SysResults[SYS_EXFLOW] = (REAL4)(StepFlowTotals.exInflow * UCF(FLOW));
    SysResults[SYS_INFLOW] = SysResults[SYS_RUNOFF] +
                             SysResults[SYS_DWFLOW] +
                             SysResults[SYS_GWFLOW] +
                             SysResults[SYS_IIFLOW] +
                             SysResults[SYS_EXFLOW];

    OutIface_saveSysResults(getDateTime(reportTime),Fout);

    // --- save outfall flows to interface file if called for
    if ( Foutflows.mode == SAVE_FILE && !IgnoreRouting ) 
        iface_saveOutletResults(reportDate, Foutflows.file);
    Nperiods++;
}

//=============================================================================

void output_end()
//
//  Input:   none
//  Output:  none
//  Purpose: writes closing records to binary file if using binary format.
//
{
    // bin_output_end();
    OutIface_output_end();
}

//=============================================================================

void output_close()
//
//  Input:   none
//  Output:  none
//  Purpose: frees memory used for accessing model results and closes sql session if using sqlite format
//
{
    FREE(SubcatchResults);
    FREE(NodeResults);
    FREE(LinkResults);
    output_closeAvgResults();   
    OutIface_output_close();
                                                  //(5.1.013)
}


//=============================================================================

void output_saveSubcatchResults(double reportTime, TFile Fout)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//           Fout = output file struct
//  Output:  none
//  Purpose: writes computed subcatchment results to output file.
//
{
    int      j;
    double   f;
    double   area;
    REAL4    totalArea = 0.0f; 
    DateTime reportDate = getDateTime(reportTime);

    // --- update reported rainfall at each rain gage
    for ( j=0; j<Nobjects[GAGE]; j++ )
    {
        gage_setReportRainfall(j, reportDate);
    }

    // --- find where current reporting time lies between latest runoff times
    f = (reportTime - OldRunoffTime) / (NewRunoffTime - OldRunoffTime);

    // --- write subcatchment results to file
    for ( j=0; j<Nobjects[SUBCATCH]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        subcatch_getResults(j, f, SubcatchResults);
        if ( Subcatch[j].rptFlag ){
            OutIface_saveSubcatchResults(Subcatch[j].ID,reportDate,Fout);
        }
        // --- update system-wide results
        area = Subcatch[j].area * UCF(LANDAREA);
        totalArea += (REAL4)area;
        SysResults[SYS_RAINFALL] +=
            (REAL4)(SubcatchResults[SUBCATCH_RAINFALL] * area);
        SysResults[SYS_SNOWDEPTH] +=
            (REAL4)(SubcatchResults[SUBCATCH_SNOWDEPTH] * area);
        SysResults[SYS_EVAP] +=
            (REAL4)(SubcatchResults[SUBCATCH_EVAP] * area);
        if ( Subcatch[j].groundwater ) SysResults[SYS_EVAP] += 
            (REAL4)(Subcatch[j].groundwater->evapLoss * UCF(EVAPRATE) * area);
        SysResults[SYS_INFIL] +=
            (REAL4)(SubcatchResults[SUBCATCH_INFIL] * area);
        SysResults[SYS_RUNOFF] += (REAL4)SubcatchResults[SUBCATCH_RUNOFF];
    }

    // --- normalize system-wide results to catchment area
    if ( totalArea > 0.0 )
    {
        SysResults[SYS_EVAP]      /= totalArea;
        SysResults[SYS_RAINFALL]  /= totalArea;
        SysResults[SYS_SNOWDEPTH] /= totalArea;
        SysResults[SYS_INFIL]     /= totalArea;
    }

    // --- update system temperature and PET
    if ( UnitSystem == SI ) f = (5./9.) * (Temp.ta - 32.0);
    else f = Temp.ta;
    SysResults[SYS_TEMPERATURE] = (REAL4)f;
    f = Evap.rate * UCF(EVAPRATE);
    SysResults[SYS_PET] = (REAL4)f;

}

//=============================================================================

////  This function was re-written for release 5.1.013.  ////                  //(5.1.013)

void output_saveNodeResults(double reportTime, TFile Fout)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//           Fout = output file struct
//  Output:  none
//  Purpose: writes computed node results to output file.
//
{
    int j;

    // --- find where current reporting time lies between latest routing times
    double f = (reportTime - OldRoutingTime) /
               (NewRoutingTime - OldRoutingTime);

    // --- write node results to file
    for (j=0; j<Nobjects[NODE]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        node_getResults(j, f, NodeResults);
        if ( Node[j].rptFlag )
            OutIface_saveNodeResults(Node[j].ID,getDateTime(reportTime),Fout);

        stats_updateMaxNodeDepth(j, NodeResults[NODE_DEPTH]);

        // --- update system-wide storage volume 
        SysResults[SYS_STORAGE] += NodeResults[NODE_VOLUME];
    }
}

//=============================================================================

void output_saveLinkResults(double reportTime,  TFile Fout)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//           Fout = output file struct
//  Output:  none
//  Purpose: writes computed link results to output file.
//
{
    int j;
    double f;
    double z;

    // --- find where current reporting time lies between latest routing times
    f = (reportTime - OldRoutingTime) / (NewRoutingTime - OldRoutingTime);

    // --- write link results to file
    for (j=0; j<Nobjects[LINK]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        if (Link[j].rptFlag)
        {
            link_getResults(j, f, LinkResults);
            OutIface_saveLinkResults(Link[j].ID,getDateTime(reportTime),Fout);
        }

        // --- update system-wide results
        z = ((1.0-f)*Link[j].oldVolume + f*Link[j].newVolume) * UCF(VOLUME);
        SysResults[SYS_STORAGE] += (REAL4)z;
    }
}



////  The following functions were added for release 5.1.013.  ////            //(5.1.013)

//=============================================================================
//  Functions for saving average results within a reporting period to file.
//=============================================================================

int output_openAvgResults()
{
    int i;
    
    // --- allocate memory for averages at reportable nodes
    AvgNodeResults = (TAvgResults *)calloc(NumNodes, sizeof(TAvgResults));
    if ( AvgNodeResults == NULL ) return FALSE;
    for (i = 0; i < NumNodes; i++ ) AvgNodeResults[i].xAvg = NULL;

    // --- allocate memory for averages at reportable links
    AvgLinkResults = (TAvgResults *)calloc(NumLinks, sizeof(TAvgResults));
    if (AvgLinkResults == NULL)
    {
        output_closeAvgResults();
        return FALSE;
    }
    for (i = 0; i < NumLinks; i++) AvgLinkResults[i].xAvg = NULL;

    // --- allocate memory for each reportable variable for each reportable node
    for (i = 0; i < NumNodes; i++)
    {
        AvgNodeResults[i].xAvg = (REAL4*) calloc(NumNodeVars, sizeof(REAL4));
        if (AvgNodeResults[i].xAvg == NULL)
        {
            output_closeAvgResults();
            return FALSE;
        }
    }

    // --- allocate memory for each reportable variable for each reportable link
    for (i = 0; i < NumLinks; i++)
    {
        AvgLinkResults[i].xAvg = (REAL4*)calloc(NumLinkVars, sizeof(REAL4));
        if (AvgLinkResults[i].xAvg == NULL)
        {
            output_closeAvgResults();
            return FALSE;
        }
    }
    return TRUE;
}

//=============================================================================

void output_closeAvgResults()
{
    int i;
    if (AvgNodeResults)
    {
        for (i = 0; i < NumNodes; i++)  FREE(AvgNodeResults[i].xAvg); 
        FREE(AvgNodeResults);
    }
    if (AvgLinkResults)
    {
        for (i = 0; i < NumLinks; i++)  FREE(AvgLinkResults[i].xAvg);
        FREE(AvgLinkResults);
    }
}

//=============================================================================

void output_initAvgResults()
{
    int i, j;
    Nsteps = 0;
    for (i = 0; i < NumNodes; i++)
    {
        for (j = 0; j < NumNodeVars; j++) AvgNodeResults[i].xAvg[j] = 0.0;
    }
    for (i = 0; i < NumLinks; i++)
    {
        for (j = 0; j < NumLinkVars; j++) AvgLinkResults[i].xAvg[j] = 0.0;
    }
}

//=============================================================================

void output_updateAvgResults()
{
    int i, j, k, sign;

    // --- update average accumulations for nodes
    k = 0;
    for (i = 0; i < Nobjects[NODE]; i++)
    {
        if ( !Node[i].rptFlag ) continue;
        node_getResults(i, 1.0, NodeResults);
        for (j = 0; j < NumNodeVars; j++)
        {
            AvgNodeResults[k].xAvg[j] += NodeResults[j];
        }
        k++;
    }

    // --- update average accumulations for links
    k = 0;
    for (i = 0; i < Nobjects[LINK]; i++)
    {
        if ( !Link[i].rptFlag ) continue;
        link_getResults(i, 1.0, LinkResults);

        // --- save sign of current flow rate
        sign = SGN(LinkResults[LINK_FLOW]);

        // --- add current results to average accumulation
        for (j = 0; j < NumLinkVars; j++)
        {
            // --- accumulate flow so its sign (+/-) will equal that of most
            //     recent flow result
            if ( j == LINK_FLOW )
            {
                AvgLinkResults[k].xAvg[j] =
                    sign * (ABS(AvgLinkResults[k].xAvg[j]) + ABS(LinkResults[j]));
            }

            // --- link capacity is another special case
            else if (j == LINK_CAPACITY)
            {
                // --- accumulate capacity (fraction full) for conduits 
                if ( Link[i].type == CONDUIT )
                    AvgLinkResults[k].xAvg[j] += LinkResults[j];

                // --- for other links capacity is pump speed or regulator
                //     opening fraction which shouldn't be averaged
                //     (multiplying by Nsteps+1 will preserve last value
                //     when average results are taken in saveAvgResults())
                else  
                    AvgLinkResults[k].xAvg[j] = LinkResults[j] * (Nsteps+1);
            }

            // --- accumulation for all other reported results
            else AvgLinkResults[k].xAvg[j] += LinkResults[j];
        }
        k++;
    }
    Nsteps++;
}

//=============================================================================

void output_saveAvgResults(double reportTime,  TFile Fout)
{
    int i, j;

    // --- examine each reportable node
    for (i = 0; i < NumNodes; i++)
    {
        // --- determine the node's average results
        for (j = 0; j < NumNodeVars; j++)
        {
            NodeResults[j] = AvgNodeResults[i].xAvg[j] / Nsteps;
        }

        // --- save average results to file
        OutIface_saveNodeResults(Node[j].ID,getDateTime(reportTime),Fout);
    }

    // --- update each node's max depth and contribution to system storage
    for (i = 0; i < Nobjects[NODE]; i++)
    {
        stats_updateMaxNodeDepth(i, Node[i].newDepth * UCF(LENGTH));
        SysResults[SYS_STORAGE] += (REAL4)(Node[i].newVolume * UCF(VOLUME));
    }

    // --- examine each reportable link
    for (i = 0; i < NumLinks; i++)
    {
        // --- determine the link's average results
        for (j = 0; j < NumLinkVars; j++)
        {
            LinkResults[j] = AvgLinkResults[i].xAvg[j] / Nsteps;
        }

        // --- save average results to file
        OutIface_saveLinkResults(Link[j].ID,getDateTime(reportTime),Fout);
    }
 
    // --- add each link's volume to total system storage
    for (i = 0; i < Nobjects[LINK]; i++)                                       //(5.1.014)
    {
        SysResults[SYS_STORAGE] += (REAL4)(Link[i].newVolume * UCF(VOLUME));
    }

    // --- re-initialize average results for all nodes and links
    output_initAvgResults();
}
