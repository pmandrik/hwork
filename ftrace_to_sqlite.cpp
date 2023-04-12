
/*
echo "function_graph" > current_tracer
echo 1 > options/funcgraph-abstime
echo 0 > options/funcgraph-overhead
g++ ftrace_to_sqlite.cpp -o exe_ftrace_to_sqlite -Wfatal-errors -lsqlite3
*/

//504.744446 |   4) + 20.329 us   |      } /* __flush_smp_call_function_queue */
//504.744446 |   4) + 20.918 us   |    } /* flush_smp_call_function_queue */
//504.744446 |   4)               |    schedule_idle() {
//504.744447 |   4)   0.306 us    |      rcu_note_context_switch();
//504.744447 |   4)               |      raw_spin_rq_lock_nested() {
//504.744447 |   4)   0.276 us    |        _raw_spin_lock();
//504.744448 |   4)   0.812 us    |      }
//504.744448 |   4)   0.284 us    |      update_rq_clock();
//504.744449 |   4)               |      pick_next_task() {


// #define DEBUG_CODE 1
#ifdef DEBUG_CODE 
#define DPRINT(x) (x)
#else 
#define DPRINT(x) do{}while(0)
#endif

#include <cstdio>
#include <vector>
#include <map>
#include <stack>
#include <string>
#include <iostream>

#include <sqlite3.h> 

using namespace std;

string quotesql( const string& s ) {
    return string("'") + s + string("'");
}

struct ftrace_entry {
  long long int id = -1;
  int cpu = -1;
  
  long long int time_start = -1;
  double duration = - 1;
  string duration_units;
  
  long long int mother_id = -1;
  string fname;
  string mother_fname;
  
  string meta = "";
  
  string _raw = "";
  
  string get_sql_header(){
    string header;
    header += "id INT PRIMARY KEY NOT NULL, ";
    header += "cpu INT, ";
    header += "duration REAL, ";;
    header += "duration_units TEXT, ";
    header += "mother_id INT, ";
    header += "fname TEXT, ";
    header += "mother_fname TEXT, ";
    header += "meta TEXT ";
    return header;
  }
  
  string get_sql_entry_header(){
    return "id, cpu, duration, duration_units, mother_id, fname, mother_fname, meta";
  }
  
  string get_sql_entry(){
    string entry;
    entry += to_string(id) + ",";
    entry += to_string(cpu) + ",";
    entry += to_string(duration) + ",";
    entry += quotesql(duration_units) + ",";
    entry += to_string(mother_id) + ",";
    entry += quotesql(fname) + ",";
    entry += quotesql(mother_fname) + ",";
    entry += quotesql(meta) + "";
    return entry;
  }
};

long long int start_time = -1;

char * skip_spaces(char * line, int max_skip = 999){
  for(int i = 0; i < max_skip; i++){
    if( line[i] == ' ' ) continue;
    return line + i;
  }
  return line + max_skip;
}

char * skip_column_edge(char * line, int max_skip = 999){
  line = skip_spaces(line, max_skip);
  line += 1;
  line = skip_spaces(line, max_skip);
  return line;
}

long long int read_numbers(char * line, int & skip_pos){
  long long int answer = 0;
  
  for(int i = 0; i < 50; i++){
    char & ch = line[i];
    if( ch == '.' ) continue;
    if( ch < '0' || ch > '9' ) { skip_pos = i; break; }
    answer = answer*10 + (ch - '0');
  }
  
  return answer;
}

double read_double(char * line, int & skip_pos){
  double answer = 0;
  
  for(int i = 0; i < 50; i++){
    char & ch = line[i];
    if( ch == '.' )  { skip_pos = i; break; }
    if( ch < '0' || ch > '9' ) { skip_pos = i; return answer; }
    answer = answer*10 + (ch - '0');
  }
  double power = 0.1;
  for(int i = 1; i < 50; i++){
    char & ch = line[i+skip_pos];
    if( ch < '0' || ch > '9' ) { skip_pos += i; return answer; }
    answer = answer + (ch - '0') * power;
    power *= 0.1;
  }
  
  return answer;
}


string read_word(char * line, int & skip_pos){
  string answer;
  
  for(int i = 0; i < 50; i++){
    char & ch = line[i];
    if( ch == ' ' ) { skip_pos = i; break; }
    answer.push_back(ch);
  }
  
  return answer;
}

ftrace_entry * create_meta_event( string & buff, const long long int & id_counter ){
    string subbuff;
    std::size_t founbuff = buff.find('\n');
    if (founbuff != std::string::npos)
      subbuff=buff.substr(0, founbuff);
    else subbuff = buff;
    
    ftrace_entry * fentry = new ftrace_entry();
    fentry->id = id_counter;
    fentry->meta = subbuff;
    return fentry;
}

stack<ftrace_entry*> fstack;

ftrace_entry * process_entry( string & buff, const long long int & id_counter ){
  DPRINT(cout << "=================== process_entry =================" << endl);
  DPRINT(cout << "raw: " << buff << endl);
  
  char * line = &buff[0];
  
  string raw;
  #ifdef DEBUG_CODE
    raw = string(line); 
    std::size_t founraw = raw.find('\n');
    if (founraw != std::string::npos)
      raw=raw.substr(0, founraw);
  #endif
  int delimeter = 0;

  line = skip_spaces(line);
  
  // normal ftrace function content 
  //     504.744449 |   4)               |      pick_next_task() {
  long long int timestamp = read_numbers( line, delimeter );
  if( timestamp == 0 ) goto unusual;
  else {
    if(start_time < 0) start_time = timestamp;
    timestamp -= start_time;
    line = line + delimeter;
    
    line = skip_column_edge(line);
    
    int cpu = read_numbers(line, delimeter);
    line = line + delimeter + 1;
    
    line = skip_spaces(line);
    double duration = read_double( line, delimeter );
    string duration_units = "";
    
    if(duration > 0){
      line = line + delimeter;
      
      line = skip_spaces(line);
      duration_units = read_word( line, delimeter );
      line = line + delimeter;
    }
    
    line = skip_column_edge(line);
    
    string fname = string(line);
    
    std::size_t found = fname.find('\n');
    if (found != std::string::npos)
      fname=fname.substr(0, found);
      
    found = fname.find("/*");
    if (found != std::string::npos)
      fname=fname.substr(0, found-1);
      
    DPRINT(cout << "timestamp = " << timestamp+start_time << " " << timestamp << endl);
    DPRINT(cout << "cpu = " << cpu << endl);
    DPRINT(cout << "duration = " << duration << endl);
    DPRINT(cout << "duration_units = " << duration_units << endl);
    DPRINT(cout << "fname = \"" << fname << "\"" << endl);
      
    // case 1. lowest function to call
    if( fname[fname.size()-1] == ';' ){
      DPRINT( cout << "ftrace_entry" << endl );
      ftrace_entry * fentry = new ftrace_entry();
      fentry->id = id_counter;
      fentry->cpu = cpu;
      fentry->time_start = timestamp;
      fentry->duration = duration;
      fentry->duration_units = duration_units;
      fentry->_raw = raw;
      
      fentry->fname = fname;
    
      if( fstack.size() ){
        ftrace_entry * fentry_prev = fstack.top();
        fentry->mother_fname = fentry_prev->fname;
        fentry->mother_id    = fentry_prev->id;
      }
    
      return fentry;
    }
    // case 2. end of mother function
    if( fname[0] == '}' ){
      DPRINT(cout << "ftrace_entry mother end" << endl );
      if( not fstack.size() ){
        DPRINT(cout << "WARNING found } without {" << endl );
        return nullptr;
      }
      
      ftrace_entry * fentry = fstack.top();
      fstack.pop();
      
      fentry->duration = duration;
      fentry->duration_units = duration_units;
      fentry->_raw = fentry->_raw + " " + raw;
    
      return fentry;
    }
    // case 3. start of mother function
    if( fname[fname.size()-1] == '{' ){
      DPRINT(cout << "ftrace_entry mother" << endl );

      ftrace_entry * fentry = new ftrace_entry();
      fentry->id = id_counter;
      fentry->cpu = cpu;
      fentry->time_start = timestamp;
      fentry->fname = fname;
      
      fentry->_raw = raw;
    
      if( fstack.size() ){
        ftrace_entry * fentry_prev = fstack.top();
        fentry->mother_fname = fentry_prev->fname;
        fentry->mother_id    = fentry_prev->id;
      }
      
      fstack.push( fentry );
      DPRINT(cout << "fstack.size() = " << fstack.size() << endl );
    
      return nullptr;
    }
    
    // unusual
    if( fname.find("=>") != std::string::npos ){
      return create_meta_event(buff, id_counter);
    }
    
    cout << "timestamp = " << timestamp+start_time << " " << timestamp << endl;
    cout << "cpu = " << cpu << endl;
    cout << "duration = " << duration << endl;
    cout << "duration_units = " << duration_units << endl;
    cout << "fname = \"" << fname << "\"" << endl;
  }
  
  unusual:
  
  if( buff.find("=>") != std::string::npos ){
    return create_meta_event(buff, id_counter);
  }
  
  if( buff.find("-------") != std::string::npos ){
    return nullptr;
  }
  
  if( buff.find("CPU:") != std::string::npos ){
    return create_meta_event(buff, id_counter);
  }
  
  // cout << string(line) << endl;
  string subbuff;
  std::size_t founbuff = buff.find('\n');
  if (founbuff != std::string::npos)
    subbuff=buff.substr(0, founbuff);
  else subbuff = buff;
  
  cout << subbuff << endl;
  cout << timestamp << endl;
  
  return nullptr;
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

void save_entry(ftrace_entry * fentry, sqlite3 *db){
  char *zErrMsg = 0;
  DPRINT( cout << "Save " << fentry->id << " " << fentry->time_start << " " << fentry->duration << " " << fentry->duration_units << " " << fentry->fname << endl );
  // cout << "Save " << fentry->id << " " << fentry->time_start << " " << fentry->duration << " " << fentry->duration_units << " " << fentry->fname << " raw=\"" << fentry->_raw << endl;
  
  string cmd = "INSERT INTO ftrace (" + fentry->get_sql_entry_header() + ") VALUES (" + fentry->get_sql_entry() + ");";
  DPRINT( cout << cmd << endl );
   
   if( sqlite3_exec(db, cmd.c_str(), callback, 0, &zErrMsg) != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   }
   
   delete fentry;
}

int main(int argc, char* argv[]) { 

  string out_name = "ftrace_test.db";
  int max_entries = 50000;
  string input_path = "/sys/kernel/tracing/trace_pipe";
  
  if(argc > 1) out_name = string(argv[1]);
  if(argc > 2) max_entries = atoi(argv[2]);
  if(argc > 3) input_path = string(argv[3]);

  sqlite3 *db;
  char *zErrMsg = 0;

  remove( out_name.c_str() );    
  if( sqlite3_open(out_name.c_str(), &db) ) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return(0);
  }
  
  string cmd = "CREATE TABLE ftrace (" + ftrace_entry().get_sql_header() + ");";
  if( sqlite3_exec(db, cmd.c_str(), callback, 0, &zErrMsg) != SQLITE_OK ){
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  
  sqlite3_exec(db, "PRAGMA journal_mode = OFF;", callback, 0, &zErrMsg);
  sqlite3_exec(db, "BEGIN TRANSACTION;", callback, 0, &zErrMsg);

  int buffer_size = 1000;
  string buffer(buffer_size, '\0');
  FILE * file = fopen(input_path.c_str(), "r");
  
  long long int id_counter = -1;
  while(true){
    
    id_counter++;
    fgets(&buffer[0], buffer_size, file);
    ftrace_entry * fentry = process_entry( buffer, id_counter );
    if(fentry){
      save_entry( fentry, db );
    } 
    
    if( id_counter % 10000 == 0 ){
      sqlite3_exec(db, "COMMIT;", callback, 0, &zErrMsg);
      sqlite3_exec(db, "BEGIN TRANSACTION;", callback, 0, &zErrMsg);
    }
    
    if(id_counter % 5000 == 0) { cout << id_counter << "/" << max_entries << endl;}
    if(id_counter > max_entries) break;
  }
  
  sqlite3_exec(db, "COMMIT;", callback, 0, &zErrMsg);
  
  sqlite3_close(db);
}








