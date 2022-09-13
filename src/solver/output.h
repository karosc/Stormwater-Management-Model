// Definition of 4-byte integer, 4-byte real and 8-byte real types
#define INT4  int
#define REAL4 float
#define REAL8 double

// Definition of 4-byte integer, 4-byte real and 8-byte real types
extern REAL4*           SubcatchResults;
extern REAL4*           NodeResults;
extern REAL4*           LinkResults;


//-----------------------------------------------------------------------------
//  Shared variables    
//-----------------------------------------------------------------------------
extern INT4      IDStartPos;           // starting file position of ID names
extern INT4      InputStartPos;        // starting file position of input data
extern INT4      OutputStartPos;       // starting file position of output data
extern INT4      BytesPerPeriod;       // bytes saved per simulation time period
extern INT4      NumSubcatchVars;      // number of subcatchment output variables
extern INT4      NumNodeVars;          // number of node output variables
extern INT4      NumLinkVars;          // number of link output variables
extern INT4      NumSubcatch;          // number of subcatchments reported on
extern INT4      NumNodes;             // number of nodes reported on
extern INT4      NumLinks;             // number of links reported on
extern INT4      NumPolluts;           // number of pollutants reported on
extern REAL4     SysResults[MAX_SYS_RESULTS];    // values of system output vars.


int output_sql_init(void);
int sql_saveSubcatchResults(char *ID,DateTime reportDate,TFile Fout);
int sql_saveNodeResults(char *ID,DateTime reportDate,TFile Fout);
int sql_saveLinkResults(char *ID,DateTime reportDate,TFile Fout);
int sql_saveSysResults(DateTime reportDate,TFile Fout);
void sql_output_end(void);
void sql_output_close(void);

void sql_readSubcatchResults(int period, int index);

int output_out_init(void);
int bin_saveSubcatchResults(char *ID,DateTime reportDate,TFile Fout);
int bin_saveNodeResults(char *ID,DateTime reportDate,TFile Fout);
int bin_saveLinkResults(char *ID,DateTime reportDate,TFile Fout);
int bin_saveSysResults(DateTime reportDate,TFile Fout);
void bin_output_end(void);
void bin_output_close(void);

void bin_saveDate(DateTime reportDate,TFile Fout);
void bin_readSubcatchResults(int period, int index);