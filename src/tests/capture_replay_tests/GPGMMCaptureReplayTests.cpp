// Copyright 2021 The GPGMM Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tests/capture_replay_tests/GPGMMCaptureReplayTests.h"

#include "gpgmm/common/Assert.h"
#include "gpgmm/common/PlatformTime.h"
#include "gpgmm/common/PlatformUtils.h"

#include <json/json.h>
#include <fstream>
#include <vector>

static const std::string kTraceIndex = GPGMM_CAPTURE_REPLAY_TESTS_TRACE_INDEX;

static std::string gSingleTraceFilePath = "";  // Always empty unless set by command-line option.

namespace {

    GPGMMCaptureReplayTestEnvironment* gTestEnv = nullptr;

    std::string LogSeverityToString(const gpgmm::LogSeverity& severity) {
        switch (severity) {
            case gpgmm::LogSeverity::Debug:
                return "DEBUG";
            case gpgmm::LogSeverity::Info:
                return "INFO";
            case gpgmm::LogSeverity::Warning:
                return "WARN";
            case gpgmm::LogSeverity::Error:
                return "ERROR";
            default:
                UNREACHABLE();
                return "";
        }
    }

    std::string AllocatorProfileToString(const AllocatorProfile& profile) {
        switch (profile) {
            case AllocatorProfile::ALLOCATOR_PROFILE_MAX_PERFORMANCE:
                return "Max Performance";
            case AllocatorProfile::ALLOCATOR_PROFILE_LOW_MEMORY:
                return "Low Memory";
            case AllocatorProfile::ALLOCATOR_PROFILE_CAPTURED:
                return "Captured";
            case AllocatorProfile::ALLOCATOR_PROFILE_DEFAULT:
                return "Default";
            default:
                UNREACHABLE();
                return "";
        }
    }

    AllocatorProfile StringToAllocatorProfile(std::string profile) {
        if (profile == "MAXPERF" || profile == "PERF" || profile == "MAX") {
            return AllocatorProfile::ALLOCATOR_PROFILE_MAX_PERFORMANCE;
        } else if (profile == "LOWMEM" || profile == "LOW" || profile == "MEM") {
            return AllocatorProfile::ALLOCATOR_PROFILE_LOW_MEMORY;
        } else if (profile == "DEFAULT" || profile == "NONE") {
            return AllocatorProfile::ALLOCATOR_PROFILE_DEFAULT;
        } else {
            return AllocatorProfile::ALLOCATOR_PROFILE_CAPTURED;
        }
    }

}  // namespace

void InitGPGMMCaptureReplayTestEnvironment(int argc, char** argv) {
    gTestEnv = new GPGMMCaptureReplayTestEnvironment(argc, argv);
    GPGMMTestEnvironment::SetEnvironment(gTestEnv);
    testing::AddGlobalTestEnvironment(gTestEnv);
}

GPGMMCaptureReplayTestEnvironment::GPGMMCaptureReplayTestEnvironment(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        constexpr const char kIterationArg[] = "--iterations=";
        size_t arglen = sizeof(kIterationArg) - 1;
        if (strncmp(argv[i], kIterationArg, arglen) == 0) {
            const char* iterations = argv[i] + arglen;
            mParams.Iterations = strtoul(iterations, nullptr, 0);
            continue;
        }

        if (strcmp("--force-standalone", argv[i]) == 0) {
            mParams.IsStandaloneOnly = true;
            continue;
        }

        if (strcmp("--never-allocate", argv[i]) == 0) {
            mParams.IsNeverAllocate = true;
            continue;
        }

        if (strcmp("--regenerate", argv[i]) == 0) {
            mParams.IsRegenerate = true;
            continue;
        }

        if (strcmp("--check-caps", argv[i]) == 0) {
            mParams.IsCapturedCapsCompat = true;
            continue;
        }

        constexpr const char kRecordLevel[] = "--record-level";
        arglen = sizeof(kRecordLevel) - 1;
        if (strncmp(argv[i], kRecordLevel, arglen) == 0) {
            const char* level = argv[i] + arglen;
            if (level[0] != '\0') {
                if (strcmp(level, "=DEBUG") == 0) {
                    mParams.RecordLevel = gpgmm::LogSeverity::Debug;
                } else if (strcmp(level, "=INFO") == 0) {
                    mParams.RecordLevel = gpgmm::LogSeverity::Info;
                } else if (strcmp(level, "=WARN") == 0) {
                    mParams.RecordLevel = gpgmm::LogSeverity::Warning;
                } else if (strcmp(level, "=ERROR") == 0) {
                    mParams.RecordLevel = gpgmm::LogSeverity::Error;
                } else {
                    gpgmm::ErrorLog() << "Invalid record log level " << level;
                    UNREACHABLE();
                }
            } else {
                mParams.RecordLevel = gpgmm::LogSeverity::Info;
            }
            continue;
        }

        constexpr const char kLogLevel[] = "--log-level";
        arglen = sizeof(kLogLevel) - 1;
        if (strncmp(argv[i], kLogLevel, arglen) == 0) {
            const char* level = argv[i] + arglen;
            if (level[0] != '\0') {
                if (strcmp(level, "=DEBUG") == 0) {
                    mParams.LogLevel = gpgmm::LogSeverity::Debug;
                } else if (strcmp(level, "=INFO") == 0) {
                    mParams.LogLevel = gpgmm::LogSeverity::Info;
                } else if (strcmp(level, "=WARN") == 0) {
                    mParams.LogLevel = gpgmm::LogSeverity::Warning;
                } else if (strcmp(level, "=ERROR") == 0) {
                    mParams.LogLevel = gpgmm::LogSeverity::Error;
                } else {
                    gpgmm::ErrorLog() << "Invalid log message level " << level << ".\n";
                    UNREACHABLE();
                }
            } else {
                mParams.LogLevel = gpgmm::LogSeverity::Warning;
            }
            continue;
        }

        constexpr const char kPlaybackFile[] = "--playback-file=";
        arglen = sizeof(kPlaybackFile) - 1;
        if (strncmp(argv[i], kPlaybackFile, arglen) == 0) {
            const char* path = argv[i] + arglen;
            if (path[0] != '\0') {
                gSingleTraceFilePath = std::string(path);
            } else {
                gpgmm::ErrorLog() << "Invalid playback file " << path << ".\n";
                UNREACHABLE();
            }
            continue;
        }

        constexpr const char kProfile[] = "--profile=";
        arglen = sizeof(kProfile) - 1;
        if (strncmp(argv[i], kProfile, arglen) == 0) {
            const char* profile = argv[i] + arglen;
            if (profile[0] != '\0') {
                mParams.AllocatorProfile = StringToAllocatorProfile(std::string(profile));
            } else {
                gpgmm::ErrorLog() << "Invalid profile " << profile << ".\n";
                UNREACHABLE();
            }
            continue;
        }

        if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0) {
            gpgmm::InfoLog()
                << "Playback options:"
                << " [--iterations=X]\n"
                << " --iterations: Number of times to run playback.\n"
                << " --record-level=[DEBUG|INFO|WARN|ERROR]: Log severity "
                   "level to record events.\n"
                << " --log-level=[DEBUG|INFO|WARN|ERROR]: Log severity "
                   "level for log messages.\n"
                << " --regenerate: Capture again upon playback.\n"
                << " --playback-file: Path to captured file to playback.\n"
                << " --caps-compatible: Captured caps must be compatible with playback device.\n";

            gpgmm::InfoLog()
                << "Experiment options:"
                << " --force-standalone: Disable memory reuse by sub-allocation.\n"
                << " --never-allocate: Disable creating backend memory.\n"
                << " --profile=[MAXPERF|LOWMEM|CAPTURED|DEFAULT]: Allocator profile.\n";
            continue;
        }
    }

    if (mParams.Iterations > 1 && mParams.IsRegenerate) {
        gpgmm::WarningLog() << "--iterations ignored when using --regenerate.\n";
        mParams.Iterations = 1;
    }

    PrintCaptureReplaySettings();
}

GPGMMCaptureReplayTestEnvironment::~GPGMMCaptureReplayTestEnvironment() = default;

void GPGMMCaptureReplayTestEnvironment::SetUp() {
    GPGMMTestEnvironment::SetUp();
    // TODO
}

void GPGMMCaptureReplayTestEnvironment::TearDown() {
    // TODO
    GPGMMTestEnvironment::TearDown();
}

void GPGMMCaptureReplayTestEnvironment::PrintCaptureReplaySettings() const {
    gpgmm::InfoLog() << "Playback settings\n"
                        "-----------------\n"
                     << "Iterations per test: " << mParams.Iterations << "\n"
                     << "Regenerate on playback: " << (mParams.IsRegenerate ? "true" : "false")
                     << "\n"
                     << "Record level: " << LogSeverityToString(mParams.RecordLevel) << "\n"
                     << "Log level: " << LogSeverityToString(mParams.LogLevel) << "\n"
                     << "Check caps: " << (mParams.IsCapturedCapsCompat ? "true" : "false") << "\n";

    gpgmm::InfoLog() << "Experiment settings\n"
                        "-------------------\n"
                     << "Force standalone: " << (mParams.IsStandaloneOnly ? "true" : "false")
                     << "\n"
                     << "Never allocate: " << (mParams.IsNeverAllocate ? "true" : "false") << "\n"
                     << "Profile: " << AllocatorProfileToString(mParams.AllocatorProfile) << "\n";
}

// static
std::vector<TraceFile> GPGMMCaptureReplayTestEnvironment::GenerateTraceFileParams() {
    // Playback only the file specified in command-line option.
    if (!gSingleTraceFilePath.empty()) {
        return {TraceFile{"SingleTrace", gSingleTraceFilePath}};
    }

    // Playback all files contained in traces folder.
    Json::Value root;
    Json::Reader reader;
    std::ifstream traceIndex(kTraceIndex, std::ifstream::binary);
    bool result = reader.parse(traceIndex, root, false);
    if (!result) {
        gpgmm::ErrorLog() << "Unable to parse: " << kTraceIndex << ".\n";
        return {};
    }

    const Json::Value& traceFilesJson = root["traceFiles"];

    std::vector<TraceFile> traceFiles;
    for (Json::Value::ArrayIndex traceFileIndex = 0; traceFileIndex < traceFilesJson.size();
         traceFileIndex++) {
        const Json::Value traceFileJson = traceFilesJson[traceFileIndex];
        traceFiles.push_back({traceFileJson["name"].asString(), traceFileJson["path"].asString()});
    }

    return traceFiles;
}

const TestEnviromentParams& GPGMMCaptureReplayTestEnvironment::GetParams() const {
    return mParams;
}

// CaptureReplayTestWithParams

CaptureReplayTestWithParams::CaptureReplayTestWithParams()
    : mPlatformTime(gpgmm::CreatePlatformTime()) {
}
void CaptureReplayTestWithParams::RunSingleTest(bool forceRegenerate,
                                                bool forceIsCapturedCapsCompat,
                                                bool forcePrefetchMemory) {
    return RunTestLoop(forceRegenerate, forceIsCapturedCapsCompat, /*forceSingleIteration*/ true,
                       forcePrefetchMemory);
}

void CaptureReplayTestWithParams::RunTestLoop(bool forceRegenerate,
                                              bool forceIsCapturedCapsCompat,
                                              bool forceSingleIteration,
                                              bool forcePrefetchMemory) {
    TestEnviromentParams envParams = gTestEnv->GetParams();
    if (forceRegenerate) {
        envParams.IsRegenerate = true;
    }

    if (forceIsCapturedCapsCompat) {
        envParams.IsCapturedCapsCompat = true;
    }

    if (forceSingleIteration) {
        envParams.Iterations = 1;
    }

    if (forcePrefetchMemory) {
        envParams.PrefetchMemory = true;
    }

    for (uint32_t i = 0; i < envParams.Iterations; i++) {
        RunTest(GetParam(), envParams, i);
    }
}

void CaptureReplayTestWithParams::LogCallStats(const std::string& name,
                                               const CaptureReplayCallStats& stats) const {
    const double avgCpuTimePerCallInMs =
        (stats.TotalCpuTime * 1e3) / ((stats.TotalNumOfCalls == 0) ? 1 : stats.TotalNumOfCalls);
    gpgmm::InfoLog() << name << " per second: " << (1e3 / avgCpuTimePerCallInMs)
                     << " (peak: " << (stats.PeakCpuTime * 1e3) << " ms)";
}

void CaptureReplayTestWithParams::LogMemoryStats(const std::string& name,
                                                 const CaptureReplayMemoryStats& stats) const {
    gpgmm::InfoLog() << name << " total "
                     << "size (bytes): " << stats.TotalSize / gTestEnv->GetParams().Iterations;

    if (stats.PeakUsage > 0) {
        gpgmm::InfoLog() << name << " peak usage (bytes): " << stats.PeakUsage;
    }

    gpgmm::InfoLog() << name << " total "
                     << "count: " << stats.TotalCount / gTestEnv->GetParams().Iterations;
}
