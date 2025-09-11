#include <sqlite3.h> 

#include "headers.h"
#include "version.h"
#include "output.h"

// local variable for doing sql inserts
sqlite3 *db;
int  rc;

char *subsql;
sqlite3_stmt *sub_insert_statement;

char *nodesql;
sqlite3_stmt *node_insert_statement;

char *linksql;
sqlite3_stmt *link_insert_statement;

char *syssql;
sqlite3_stmt *sys_insert_statement;

char * sErrMsg;



char * append_strings(const char * old, const char * new)
//
//https://stackoverflow.com/questions/2468421/how-to-dynamically-expand-a-string-in-c
//
{
    // find the size of the string to allocate
    size_t len = strlen(old) + strlen(new) + 1;

    // allocate a pointer to the new string
    char *out = malloc(len);

    // concat both strings and return
    sprintf(out, "%s%s", old, new);

    return out;
}


static int callback(void *NotUsed, int argc, char **argv, char **azColName)
//
//  callback function for using with sqlite3_exec. Not totally sure what should be put in here
//  but there needs to be a callback for the sqlite3_exec call and found working example here:
//  https://github.com/tjt7a/masquerade/blob/master/src/sqlite_example.c
//
 {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}


int sql_saveSubcatchResults(char *ID,DateTime reportDate,TFile Fout)

{        
    
    char   timeStamp[24]; 
    datetime_getTimeStamp(ISO, reportDate, 24, timeStamp);
    sqlite3_bind_text(sub_insert_statement,1, timeStamp,-1,0);
    sqlite3_bind_text(sub_insert_statement,2, ID,-1,0);

    int i;
    for (i=0;i<NumSubcatchVars;i++){
        sqlite3_bind_double(sub_insert_statement,i+3,SubcatchResults[i]);
    }

    sqlite3_step(sub_insert_statement);
    sqlite3_clear_bindings(sub_insert_statement);
    // Reset the statement
    sqlite3_reset(sub_insert_statement);

    return 0;
}

int sql_saveNodeResults(char *ID,DateTime reportDate,TFile Fout){        
    
    char   timeStamp[24]; 
    datetime_getTimeStamp(ISO, reportDate, 24, timeStamp);
    sqlite3_bind_text(node_insert_statement,1, timeStamp,-1,0);
    sqlite3_bind_text(node_insert_statement,2, ID,-1,0);
    
    int i;
    for (i=0;i<NumNodeVars;i++){
        sqlite3_bind_double(node_insert_statement,i+3,NodeResults[i]);
    }

    sqlite3_step(node_insert_statement);
    sqlite3_clear_bindings(node_insert_statement);
    // Reset the statement
    sqlite3_reset(node_insert_statement);

    return 0;
}

int sql_saveLinkResults(char *ID,DateTime reportDate,TFile Fout){        
    
    char   timeStamp[24]; 
    datetime_getTimeStamp(ISO, reportDate, 24, timeStamp);
    sqlite3_bind_text(link_insert_statement,1, timeStamp,-1,0);
    sqlite3_bind_text(link_insert_statement,2, ID,-1,0);

    int i;
    for (i=0;i<NumLinkVars;i++){
        sqlite3_bind_double(link_insert_statement,i+3,LinkResults[i]);
    }

    sqlite3_step(link_insert_statement);
    sqlite3_clear_bindings(link_insert_statement);
    // Reset the statement
    sqlite3_reset(link_insert_statement);

    return 0;
}

int sql_saveSysResults(DateTime reportDate,TFile Fout){        
    
    char   timeStamp[24]; 
    datetime_getTimeStamp(ISO, reportDate, 24, timeStamp);
    sqlite3_bind_text(sys_insert_statement,1, timeStamp,-1,0);

    int i;
    for (i=0;i<MAX_SYS_RESULTS;i++){
        sqlite3_bind_double(sys_insert_statement,i+2,SysResults[i]);
    }

    sqlite3_step(sys_insert_statement);
    sqlite3_clear_bindings(sys_insert_statement);
    // Reset the statement
    sqlite3_reset(sys_insert_statement);

    return 0;
}


int output_sql_init(){
    int   j;
    int   m;

    char *zErrMsg = 0;
    char *sql;
    INT4  k;
    REAL4 x;
    REAL8 z;

    
    // delete existing db and create new one
    remove(Fout.name);
    rc = sqlite3_open(Fout.name, &db);
    sErrMsg = 0;

    // log database creations error
    // TODO:  find a way to kill model if this happens
    if( rc ) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return(0);
   } else {
        fprintf(stderr, "\nOpened database successfully\n");
   }
  
    // --- ignore pollutants if no water quality analsis performed
    if ( IgnoreQuality ) NumPolluts = 0;
    else NumPolluts = Nobjects[POLLUT];


     /* Create SQL statement */
   sql = " CREATE TABLE IF NOT EXISTS subcatch_results ("\
                "datetime    DATETIME,"\
                "subcatchment    TEXT,"\
                "rainfall    REAL,"\
                "snowdepth    REAL,"\
                "evap    REAL,"\
                "infil    REAL,"\
                "runoff    REAL,"\
                "gwflow    REAL,"\
                "gwelev    REAL,"\
                "gwsat    REAL"\
            ");"\
            "CREATE TABLE IF NOT EXISTS node_results ("\
                "datetime    DATETIME,"\
                "node    TEXT,"\
                "depth    REAL,"\
                "head    REAL,"\
                "volume    REAL,"\
                "latinflow    REAL,"\
                "totalinflow    REAL,"\
                "flood    REAL"\
            ");"\
            "CREATE TABLE IF NOT EXISTS link_results ("\
                "datetime    DATETIME,"\
                "link    TEXT,"\
                "depth    REAL,"\
                "flow    REAL,"\
                "velocity    REAL,"\
                "volume    REAL,"\
                "capacity    REAL"\
            ");"
            "CREATE TABLE IF NOT EXISTS sys_results ("\
                "datetime    DATETIME,"\
                "temp    REAL,"\
                "rain    REAL,"\
                "snowdepth    REAL,"\
                "infil    REAL,"\
                "runoff    REAL,"\
                "dwflow    REAL,"\
                "gwflow    REAL,"\
                "iiflow    REAL,"\
                "exflow    REAL,"\
                "inflow    REAL,"\
                "flood    REAL,"\
                "outflow    REAL,"\
                "storage    REAL,"\
                "evap    REAL,"\
                "pet    REAL"\
            ");";
    
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

    // log database setup error
    // TODO:  find a way to kill model if this happens
    if( rc != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   } else {
      fprintf(stdout, "Tables created successfully\n");
   }


    // add pollutent columns to database.
    // prefix pollutant names to avoid column name collisions 
    // e.g. pollutant tracer on rainfall called "rainfall"
    char ssql[100];
    int p;
    for (p = 0; p < NumPolluts; p++){
        sprintf(ssql, "ALTER TABLE subcatch_results ADD COLUMN pol_%s REAL", Pollut[p].ID);
        rc = sqlite3_exec(db, ssql, callback, 0, &zErrMsg);

        sprintf(ssql, "ALTER TABLE node_results ADD COLUMN pol_%s REAL", Pollut[p].ID);
        rc = sqlite3_exec(db, ssql, callback, 0, &zErrMsg);

        sprintf(ssql, "ALTER TABLE link_results ADD COLUMN pol_%s REAL", Pollut[p].ID);
        rc = sqlite3_exec(db, ssql, callback, 0, &zErrMsg);
    }


    // is there a better way to prepare insert statments here?
    int sublen = 39 + NumSubcatchVars * 2 + 2 + 1 + 5;
    char *subsql = malloc(sublen*sizeof(char));
    strcpy(subsql,"INSERT INTO subcatch_results VALUES(?,?,?,?,?,?,?,?,?,?");

    int nodelen = 35 + NumNodeVars * 2 + 2 + 1 + 5;
    char *nodesql = malloc(nodelen*sizeof(char));
    strcpy(nodesql,"INSERT INTO node_results VALUES(?,?,?,?,?,?,?,?");

    int linklen = 35 + NumLinkVars * 2 + 2 + 1 + 5;
    char *linksql = malloc(linklen*sizeof(char));
    strcpy(linksql,"INSERT INTO link_results VALUES(?,?,?,?,?,?,?");

    syssql = "INSERT INTO sys_results VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);\0";


    // add param place holders for pollutants in insert statements
    int i;
    for (i=0;i<NumPolluts;i++){

      strcat(nodesql,",?");
      strcat(subsql,",?");
      strcat(linksql,",?");
      continue;
    }

    //close out insert statement
    strcat(nodesql,");\0");
    strcat(subsql,");\0");
    strcat(linksql,");\0");

    //prepare reusable insert statement for each table
    sqlite3_prepare_v2(db, subsql, -1, &sub_insert_statement, NULL);
    sqlite3_prepare_v2(db, nodesql, -1, &node_insert_statement, NULL);
    sqlite3_prepare_v2(db, linksql, -1, &link_insert_statement, NULL);
    sqlite3_prepare_v2(db, syssql, -1, &sys_insert_statement, NULL);

    // start transaction during which all data is written
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg);

    // TODO: maybe add some error code logic?
    return 0;

}

void sql_output_end(){
    
}

void sql_output_close(){
    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &sErrMsg);
    sqlite3_close(db); 
}

void sql_readSubcatchResults(int period,int index){
    double reportTime;
    reportTime = ReportStep * period * 1000;
    char   timeStamp[24]; 
    DateTime reportDate = getDateTime(reportTime);
    datetime_getTimeStamp(ISO, reportDate, 24, timeStamp);
    fprintf(stderr, "Datetime: %s \n",timeStamp);
}