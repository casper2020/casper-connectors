/**
 * @file process.cc
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sys/process.h"

#include <errno.h>    // errno
#include <sys/stat.h> // stat
#include <signal.h>   // kill
#include <unistd.h>   // unlink, readlink
#include <string>
#include <sstream>
#include <limits.h>   // PATH_MAX

#ifdef __APPLE__
  #include <libproc.h>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wextra-semi"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "lemon/topology_sort.h"
#pragma GCC diagnostic pop

#include "cc/exception.h"

/**
 * @brief Default constructor.
 *
 * @param a_info
 */
sys::Process::Process (sys::Process::Info&& a_info)
    : info_(a_info), pid_(0)
{
    argv_ = nullptr;
    argc_ = 0;
    
    if ( info_.path_.size() > 0 && '/' != info_.path_.c_str()[info_.path_.size()-1] ) {
        uri_ = info_.path_ + '/' + info_.executable_;
    } else {
        uri_ = info_.path_ + info_.executable_;
    }
    
    std::vector<std::string> args;
    std::stringstream ss;
    std::string arg;
    ss.str(info_.arguments_);
    while (std::getline(ss, arg, ' ')) {
        args.push_back(arg);
    }
    (*this) = args;
}

/**
 * @brief Destructor
 */
sys::Process::~Process ()
{
    if ( nullptr != argv_ ) {
        for ( size_t idx = 0 ; idx < sizeof(argv_) / sizeof(argv_[0]) ; ++idx ) {
            if ( nullptr != argv_[idx] ) {
                free(argv_[idx]);
            }
        }
        free(argv_);
    }
}

/**
 * @brief Send a signal to a child process.
 *
 * @param a_signal_no Signal number to send.
 *
 * @param a_optional If true, any error will be ignored.
 *
 * @return True on success, false otherwise.
 */
bool sys::Process::Signal (const int a_signal_no, const bool a_optional)
{
    if ( 0 == pid_ ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  errno,
                  "failed to a signal to '%s', pid not set!", info().id_.c_str()
        );
        return false;
    }
    
    // ... try to signal process ...
    if ( 0 != kill(pid(), a_signal_no) && false == a_optional ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  errno,
                  "failed to a signal to '%s', with pid %d!", info().id_.c_str(), pid_
        );
    }
        
    // ... done ...
    return ( false == is_error_set() );
}

/**
 * @brief Kill a process.
 *
 * @param a_optional If true, any error will be ignored.
 *
 * @return True on success, false otherwise.
 */
bool sys::Process::Kill (const bool a_optional)
{
    return Signal(SIGKILL, a_optional);
}

/**
 * @brief Terminate a process.
 *
 * @param a_optional If true, any error will be ignored.
 *
 * @return True on success, false otherwise.
 */
bool sys::Process::Terminate (const bool a_optional)
{
    return Signal(SIGTERM, a_optional);
}

/**
 * @brief Write a PID to a file.
 *
 * @return True on success, false otherwise.
 */
bool sys::Process::WritePID ()
{
    const char* const uri = info_.pid_file_.c_str();
    
    FILE* file = fopen(uri, "w");
    if ( nullptr == file ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  errno,
                  "Unable to write to pid file %s!", uri
        );
        return false;
    }
    
    char buffer[20] = {0};
    const int max = sizeof(buffer) / sizeof(buffer[0]) - 1;
    const int aux = snprintf(buffer, max, "%d", pid());
    if ( aux < 0 || aux > max ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  errno,
                  "Unable to write to pid file %s: buffer write error!", uri
        );
        return false;
    }
    
    fwrite(buffer, sizeof(char), aux, file);
    fclose(file);
    
    return true;
}

/**
 * @brief Read a PID from a file.
 *
 * @param a_optional If true, any error will be ignored.
 *
 * @param o_pid PID read from file, 0 if none.
 *
 * @return True on success, false otherwise.
 */
bool sys::Process::ReadPID (const bool a_optional, pid_t& o_pid)
{
    const char* const pid_file_uri = info_.pid_file_.c_str();
    
    struct stat stat_info;
    if ( -1 == stat(pid_file_uri, &stat_info) ) {
        // ... unable to access pid file ...
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  sys::Error::k_no_error_,
                  "Cannot read '%s' pid - unable to access file '%s'!", uri().c_str(), pid_file_uri
        );
        return false;
    } else if ( 0 == S_ISREG(stat_info.st_mode) ) {
        // ... file does not exist ...
        if ( false == a_optional ) {
            // ... unable to access pid file ...
            SetError (this, __FILE__, __FUNCTION__, __LINE__,
                      sys::Error::k_no_error_,
                      "Cannot read '%s' pid - file '%s' does not exist!", uri().c_str(), pid_file_uri
            );
        }
        // ... we're done ...
        return ( a_optional ? true : ( false == is_error_set()) );
    } /* else { // ... file exists, fallthrough ... } */
    
    // ... read from file ...
    FILE* file = fopen(pid_file_uri, "r");
    if ( nullptr == file ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  errno,
                  "Cannot kill %s - unable to open file '%s' !", uri().c_str(), pid_file_uri
        );
    } else {
        char buffer[20] = {0};
        const size_t max = sizeof(buffer) / sizeof(buffer[0]) - 1;
        const size_t aux = fread(buffer, sizeof(char), max, file);
        if ( 0 == aux ) {
            SetError (this, __FILE__, __FUNCTION__, __LINE__,
                      errno,
                      "Cannot kill '%s' - unable to read '%s'!", uri().c_str(), pid_file_uri
            );
        } else {
            if ( 1 != sscanf(buffer, "%d", &o_pid) ) {
                SetError (this, __FILE__, __FUNCTION__, __LINE__,
                          errno,
                          "Cannot kill '%s' - unable to scan '%s'!", uri().c_str(), pid_file_uri
                );
            }/* else { // ... pid read ... } */
        }
        fclose(file);
    }
    
    // ... done ...
    return ( false == is_error_set() );
}


/**
 * @brief Unlink a PID file.
 *
 * @param a_optional If true, any error will be ignored.
 *
 * @return True on success, false otherwise.
 */
bool sys::Process::UnlinkPID (const bool a_optional)
{
    const char* const pid_file_uri = info_.pid_file_.c_str();
    
    struct stat stat_info;
    if ( -1 == stat(pid_file_uri, &stat_info) ) {
        // ... unable to access pid file ..
        if ( false == a_optional ) {
            SetError (this, __FILE__, __FUNCTION__, __LINE__,
                      sys::Error::k_no_error_,
                      "Cannot unlink '%s' pid - unable to access file '%s'!", uri().c_str(), pid_file_uri
            );
            // ... failure ...
            return true;
        } else {
            // ... ignored, done ...
            return true;
        }
    } else if ( 0 == S_ISREG(stat_info.st_mode) ) {
        // ... file does not exist ...
        if ( false == a_optional ) {
            // ... unable to access pid file ...
            SetError (this, __FILE__, __FUNCTION__, __LINE__,
                      sys::Error::k_no_error_,
                      "Cannot unlink '%s' pid - file '%s' does not exist!", uri().c_str(), pid_file_uri
            );
            // ... failure ...
            return true;
        } else {
            // ... ignored, done ...
            return true;
        }
    } /* else { // ... file exists, fallthrough ... } */
    
    // ... unlink file ...
    if ( -1 == unlink(pid_file_uri) ) {
        if ( errno == ENOENT ) {
            // ... file does not exist, done ...
            return true;
        } else {
            // ... unable to unlink pid file ...
            SetError (this, __FILE__, __FUNCTION__, __LINE__,
                      sys::Error::k_no_error_,
                      "Cannot unlink '%s' pid - file '%s'!", uri().c_str(), pid_file_uri
            );
            // ... failure ...
            return false;
        }
    }
    
    // ... done ...
    return true;
}

/**
 * @brief Try to load a PID from the file.
 *
 * @param a_optional If true, any error will be ignored.
 *
 * @return True on success, false otherwise.
*/
bool sys::Process::LoadPIDFromFile (const bool a_optional)
{
    return ReadPID(a_optional, pid_);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Sort a vector of processes.
 *
 * @param a_vector
 * @param a_list
 */
void sys::Process::Sort (const std::vector<sys::Process::Info>& a_vector,
                         std::list<sys::Process::Info>& o_list)
{
    //
    // ... dependencies check ...
    //
    lemon::ListDigraph                              dep_graph;
    lemon::ListDigraph::ArcMap<int>                 arc_cost(dep_graph);
    lemon::ListDigraph::NodeMap<int>                ordered(dep_graph);
    std::map<std::string, lemon::ListDigraph::Node> nodes_to_names;
    
    
    struct Dependency {
        const std::string              id_;
        const std::vector<std::string> precedents_;
    };
    
    // ... first iteration, prepare required data for analysis ...
    std::vector<struct Dependency> dependencies;
    for ( auto it : a_vector ) {
        dependencies.push_back({
            /* id_         */ it.id_,
            /* precedents_ */ it.depends_on_
        });
        nodes_to_names[it.id_] = dep_graph.addNode();
    }
    
    // ... calculate costs ...
    for ( auto dependency: dependencies ) {
        auto& node = nodes_to_names[dependency.id_];
        for ( auto precedents_it = dependency.precedents_.begin(); dependency.precedents_.end() != precedents_it ; ++precedents_it ) {
            auto iterator = nodes_to_names.find(*precedents_it);
            if ( nodes_to_names.end() != iterator ) {
                arc_cost.set(dep_graph.addArc(node, iterator->second), 1);
            }
        }
    }
    
    // ... check if there are circular dependencies...
    if ( lemon::checkedTopologicalSort(dep_graph, ordered) == false ) {
        
        lemon::Path<lemon::ListDigraph>                                              path;
        lemon::HartmannOrlinMmc<lemon::ListDigraph, lemon::ListDigraph::ArcMap<int>> hart(dep_graph, arc_cost);
        
        hart.cycle(path).run();
        
        std::string reason = "\t + Found circular dependency in:\n";
        
        for ( int i = 0; i < path.length() ; ++i ) {
            const auto                    item = a_vector.begin() + dep_graph.id(dep_graph.source(path.nth(i)));
            reason += "\t\t - Process: " + item->id_ + " <=";
            if ( item->depends_on_.size() > 0 ) {
                for ( auto d : item->depends_on_ ) {
                    reason += " " + d + "," ;
                }
                reason.pop_back();
            }
            reason += "\n";
        }
        
        throw ::cc::Exception("An error occurred while sorting processes list:\n%s", reason.c_str());
        
    }
        
    std::map<size_t, const sys::Process::Info*> map;
    
    const size_t formula_count = a_vector.size();
    for ( lemon::ListDigraph::NodeIt n(dep_graph); n != lemon::INVALID; ++n ) {
        map[( ( formula_count - ordered[n] ) - 1 )] = &(*(a_vector.begin() + dep_graph.id(n)));
    }
    
    for ( size_t idx = 0 ; idx < map.size(); ++idx ) {
        o_list.push_back(*(map.find(idx)->second));
    }
    
    map.clear();
}

/**
 * @brief Obtain an process executable file URI.
 *
 * @param a_pid Process pid.
 *
 * @return Local executable URI for provided pid.
 */
std::string sys::Process::GetExecURI (const pid_t& a_pid)
{
#ifdef __APPLE__
    char buffer[PROC_PIDPATHINFO_MAXSIZE];

    const int rv = proc_pidpath(a_pid, buffer, sizeof(buffer) / sizeof(buffer[9]));
    if ( rv <= 0 ) {
        throw ::cc::Exception("An error occurred while trying to obtain process executable path: (%d) %s ", errno, strerror(errno));
    }
    return std::string(buffer);
#else
    char uri[PATH_MAX/2] ; uri[0] = '\0';
    const int aux = snprintf(uri, (PATH_MAX/2)-1, "/proc/%u/exe", a_pid);
    if ( aux < 0 ) {
        throw ::cc::Exception("An error occurred while trying to obtain process executable path: (%d) %s ", errno, strerror(errno));
    } else if ( aux >= (PATH_MAX/2)-1 ) {
        throw ::cc::Exception("An error occurred while trying to obtain process executable path: (%d) %s ", aux, "buffer to short to set proc URI");
    }
    char buffer[PATH_MAX];
    ssize_t len;
    if ( -1 == ( len = readlink(uri, buffer, PATH_MAX-1) ) ) {
        throw ::cc::Exception("An error occurred while trying to obtain process executable path: (%d) %s ", errno, strerror(errno));
    } else if ( (PATH_MAX-1-1) == len ) {
        throw ::cc::Exception("An error occurred while trying to obtain process executable path: (%d) %s ", aux, "buffer to short to write URI");
    } else {
        return std::string(buffer, len);
    }
#endif
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Set an error.
 *
 * @param a_process
 * @param o_error
 * @param a_file
 * @param a_function
 * @param a_line
 * @param a_errno
 * @param a_format
 * @param ...
 *
 * @return True on success, false otherwise.
 */
void sys::Process::SetError (const ::sys::Process* /* a_process */,
                             const char* const /* a_file */, const char* const a_function, const int a_line,
                             const errno_t a_errno, const char* const a_format, ...)
{
    error_.Reset();
    
    error_ = a_errno;
    auto temp   = std::vector<char> {};
    auto length = std::size_t { 512 };
    std::va_list args;
    while ( temp.size() <= length ) {
        temp.resize(length + 1);
        va_start(args, a_format);
        const auto status = std::vsnprintf(temp.data(), temp.size(), a_format, args);
        va_end(args);
        if ( status < 0 ) {
            throw ::cc::Exception("An error occurred while writing an error message to a buffer!");
        }
        length = static_cast<std::size_t>(status);
    }
    error_ = length > 0 ? std::string { temp.data(), length } : 0;
    error_ = std::make_pair(a_function, a_line);
}
