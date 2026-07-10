/*
 * Per-process GPU utilization types for nvmlDeviceGetProcessesUtilizationInfo
 * (driver 580+). Omitted from older CUDA nvml.h headers shipped with the
 * project build; skipped when a newer nvml.h already defines them.
 */
#ifndef SRC_INCLUDE_NVML_PROCESSES_UTILIZATION_SUBSET_H_
#define SRC_INCLUDE_NVML_PROCESSES_UTILIZATION_SUBSET_H_

#include <stdint.h>

#ifndef nvmlProcessesUtilizationInfo_v1

/**
 * Structure to store utilization value and process Id -- version 1
 */
typedef struct nvmlProcessUtilizationInfo_v1_st {
  uint64_t timeStamp;  //!< CPU Timestamp in microseconds
  unsigned int pid;              //!< PID of process
  unsigned int smUtil;           //!< SM (3D/Compute) Util Value
  unsigned int memUtil;          //!< Frame Buffer Memory Util Value
  unsigned int encUtil;          //!< Encoder Util Value
  unsigned int decUtil;          //!< Decoder Util Value
  unsigned int jpgUtil;          //!< Jpeg Util Value
  unsigned int ofaUtil;          //!< Ofa Util Value
} nvmlProcessUtilizationInfo_v1_t;

/**
 * Structure to store utilization and process ID for each running process -- version 1
 */
typedef struct nvmlProcessesUtilizationInfo_v1_st {
  unsigned int version;            //!< The version number of this struct
  unsigned int processSamplesCount;  //!< Caller array size / returned count
  uint64_t lastSeenTimeStamp;  //!< Min timestamp for returned samples
  nvmlProcessUtilizationInfo_v1_t *procUtilArray;  //!< Caller-allocated samples
} nvmlProcessesUtilizationInfo_v1_t;

typedef nvmlProcessesUtilizationInfo_v1_t nvmlProcessesUtilizationInfo_t;

#define nvmlProcessesUtilizationInfo_v1 \
    ((unsigned int)(sizeof(nvmlProcessesUtilizationInfo_v1_t) | (1U << 24U)))

nvmlReturn_t nvmlDeviceGetProcessesUtilizationInfo(
    nvmlDevice_t device, nvmlProcessesUtilizationInfo_t *processesUtilInfo);

#endif /* nvmlProcessesUtilizationInfo_v1 */

#endif  // SRC_INCLUDE_NVML_PROCESSES_UTILIZATION_SUBSET_H_
