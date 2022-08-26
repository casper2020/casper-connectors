/**
 * @file types.h
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_EASY_JOB_TYPES_H_
#define NRS_CC_EASY_JOB_TYPES_H_

#define CC_JOB_LOG_LEVEL_CRT 1 // CRITICAL
#define CC_JOB_LOG_LEVEL_ERR 2 // ERROR
#define CC_JOB_LOG_LEVEL_WRN 3 // WARNING
#define CC_JOB_LOG_LEVEL_INF 4 // INFO
#define CC_JOB_LOG_LEVEL_VBS 5 // VERBOSE
#define CC_JOB_LOG_LEVEL_DBG 6 // DEBUG
#define CC_JOB_LOG_LEVEL_PRN 7 // PARANOID

#define CC_JOB_LOG_STEP_IN        "IN"
#define CC_JOB_LOG_STEP_OUT       "OUT"
#define CC_JOB_LOG_STEP_REDIS     "REDIS"
#define CC_JOB_LOG_STEP_POSGRESQL "PGSQL"
#define CC_JOB_LOG_STEP_HTTP      "HTTP"
#define CC_JOB_LOG_STEP_FILE      "FILE"
#define CC_JOB_LOG_STEP_BEANSTALK "BT"
#define CC_JOB_LOG_STEP_STEP      "STEP"
#define CC_JOB_LOG_STEP_INFO      "INFO"
#define CC_JOB_LOG_STEP_STATUS    "STATUS"
#define CC_JOB_LOG_STEP_STATS     "STATS"
#define CC_JOB_LOG_STEP_RELAY     "RELAY"
#define CC_JOB_LOG_STEP_TTR       "TTR"
#define CC_JOB_LOG_STEP_VALIDITY  "VALIDITY"
#define CC_JOB_LOG_STEP_TIMEOUT   "TIMEOUT"
#define CC_JOB_LOG_STEP_WARNING   "WARNING"
#define CC_JOB_LOG_STEP_ALERT     "ALERT"
#define CC_JOB_LOG_STEP_RTT       "RTT"
#define CC_JOB_LOG_STEP_ERROR     "ERROR"
#define CC_JOB_LOG_STEP_V8        "V8"
#define CC_JOB_LOG_STEP_DUMP      "DUMP"

namespace cc
{

    namespace easy
    {

        namespace job
        {
        
            typedef struct {
                const std::string                        key_;
                const std::map<std::string, Json::Value> arguments_;
            } I18N;

            typedef struct {
                const char* const code_;
                const std::string why_;
            } InternalError;
            
            typedef struct {
                const char* const content_type_;
                const char* const code_;
                const std::string why_;
            } Error;

            typedef struct {
                const char* const     code_;
                const std::exception& excpt_;
            } InternalException;
        
        } // end of namespace 'job'
    
    } // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_JOB_TYPES_H_
