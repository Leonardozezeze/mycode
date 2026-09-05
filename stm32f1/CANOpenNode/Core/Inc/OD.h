/*
 * Object Dictionary for CANopenNode
 *
 * This file defines the Object Dictionary for the CANopen device.
 * It contains all communication and device parameters accessible via CANopen.
 */

#ifndef OD_H
#define OD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "301/CO_ODinterface.h"
#include <stdint.h>
#include <stddef.h>

/*
 * OD configuration counts - used by CANopenNode to allocate resources
 */
#define OD_CNT_NMT          1
#define OD_CNT_EM           1
#define OD_CNT_EM_PROD      1
#define OD_CNT_HB_PROD      1
#define OD_CNT_HB_CONS      0
#define OD_CNT_ARR_1016     0
#define OD_CNT_SDO_SRV      1
#define OD_CNT_SDO_CLI      0
#define OD_CNT_TIME         0
#define OD_CNT_SYNC         1
#define OD_CNT_RPDO         4
#define OD_CNT_TPDO         4
#define OD_CNT_LEDS         1
#define OD_CNT_LSS_SLV      1
#define OD_CNT_LSS_MST      0
#define OD_CNT_GFC          0
#define OD_CNT_SRDO         0
#define OD_CNT_ARR_1003     8

/*
 * Storage groups for Object Dictionary variables
 */

/* PERSIST_COMM - Communication parameters stored in non-volatile memory */
typedef struct {
    uint32_t x1000_deviceType;
    struct {
        uint8_t  maxSubIndex;
        uint32_t vendor_ID;
        uint32_t productCode;
        uint32_t revisionNumber;
        uint32_t serialNumber;
    } x1018_identity;
    uint32_t x1005_syncCOBID;
    uint32_t x1006_communicationCyclePeriod;
    uint32_t x1007_synchronousWindowLength;   /* DS-301: UNSIGNED32 */
    uint16_t x1017_producerHeartbeatTime;
    uint32_t x1014_cobIDEmergency;
    uint16_t x1015_inhibitTimeEmergency;
    struct {
        uint8_t  maxSubIndex;
        uint32_t consumerHeartbeatTime[1];
    } x1016_consumerHeartbeatTime;
    struct {
        uint8_t  maxSubIndex;
        uint32_t parameter[1];
    } x1010_storeParameters;
    struct {
        uint8_t  maxSubIndex;
        uint32_t parameter[1];
    } x1011_restoreDefaultParameters;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1800_tpdo1Param;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1801_tpdo2Param;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1802_tpdo3Param;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1803_tpdo4Param;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1400_rpdo1Param;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1401_rpdo2Param;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1402_rpdo3Param;
    struct {
        uint8_t  maxSubIndex;
        uint32_t COBID;         /* sub1: COB-ID, bit31=1 disables PDO */
        uint8_t  transmissionType;
        uint16_t inhibitTime;
        uint16_t eventTimer;
        uint8_t  SYNCstartValue;
    } x1403_rpdo4Param;
} OD_PERSIST_COMM_t;

/* RAM - Runtime variables (not stored in non-volatile memory) */
typedef struct {
    uint8_t  x1001_errorRegister;
    uint8_t  x1003_preDefinedErrorField_sub0;
    uint32_t x1003_preDefinedErrorField[8];
    uint32_t x1002_manufacturerStatusRegister;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1A00_tpdo1Mapping;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1A01_tpdo2Mapping;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1A02_tpdo3Mapping;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1A03_tpdo4Mapping;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1600_rpdo1Mapping;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1601_rpdo2Mapping;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1602_rpdo3Mapping;
    struct {
        uint8_t  numberOfMappedObjects; /* sub0: number of mapped objects */
        uint32_t mappedObject[4];       /* sub1..4: mapping entries */
    } x1603_rpdo4Mapping;
    /* Application variables can be added here */
    uint8_t  x2000_applicationVariable1;
    uint16_t x2001_applicationVariable2;
    uint32_t x2002_applicationVariable3;
} OD_RAM_t;

/* Global OD variable instances */
extern OD_PERSIST_COMM_t OD_PERSIST_COMM;
extern OD_RAM_t          OD_RAM;

/* Object Dictionary pointer */
extern OD_t *OD;

/*
 * OD entry shortcuts - direct access to specific OD entries
 */
#define OD_ENTRY_H1000    &OD->list[0]
#define OD_ENTRY_H1001    &OD->list[1]
#define OD_ENTRY_H1002    &OD->list[2]
#define OD_ENTRY_H1003    &OD->list[3]
#define OD_ENTRY_H1005    &OD->list[4]
#define OD_ENTRY_H1006    &OD->list[5]
#define OD_ENTRY_H1007    &OD->list[6]
#define OD_ENTRY_H1014    &OD->list[7]
#define OD_ENTRY_H1015    &OD->list[8]
#define OD_ENTRY_H1016    &OD->list[9]
#define OD_ENTRY_H1017    &OD->list[10]
#define OD_ENTRY_H1018    &OD->list[11]
#define OD_ENTRY_H1200    &OD->list[12]
#define OD_ENTRY_H1400    &OD->list[13]
#define OD_ENTRY_H1401    &OD->list[14]
#define OD_ENTRY_H1402    &OD->list[15]
#define OD_ENTRY_H1403    &OD->list[16]
#define OD_ENTRY_H1600    &OD->list[17]
#define OD_ENTRY_H1601    &OD->list[18]
#define OD_ENTRY_H1602    &OD->list[19]
#define OD_ENTRY_H1603    &OD->list[20]
#define OD_ENTRY_H1800    &OD->list[21]
#define OD_ENTRY_H1801    &OD->list[22]
#define OD_ENTRY_H1802    &OD->list[23]
#define OD_ENTRY_H1803    &OD->list[24]
#define OD_ENTRY_H1A00    &OD->list[25]
#define OD_ENTRY_H1A01    &OD->list[26]
#define OD_ENTRY_H1A02    &OD->list[27]
#define OD_ENTRY_H1A03    &OD->list[28]
#define OD_ENTRY_H1010    &OD->list[29]
#define OD_ENTRY_H1011    &OD->list[30]
#define OD_ENTRY_H1012    NULL  /* TIME not used */
#define OD_ENTRY_H1010_storeParameters  OD_ENTRY_H1010
#define OD_ENTRY_H1011_restoreDefaultParameters  OD_ENTRY_H1011

/* Helper macro for CO_MULTIPLE_OD configuration */
#define OD_INIT_CONFIG(co_config) \
    do { \
        (co_config).CNT_NMT = OD_CNT_NMT; \
        (co_config).ENTRY_H1017 = OD_ENTRY_H1017; \
        (co_config).CNT_HB_CONS = OD_CNT_HB_CONS; \
        (co_config).CNT_ARR_1016 = OD_CNT_ARR_1016; \
        (co_config).ENTRY_H1016 = OD_ENTRY_H1016; \
        (co_config).CNT_EM = OD_CNT_EM; \
        (co_config).ENTRY_H1001 = OD_ENTRY_H1001; \
        (co_config).ENTRY_H1014 = OD_ENTRY_H1014; \
        (co_config).ENTRY_H1015 = OD_ENTRY_H1015; \
        (co_config).CNT_ARR_1003 = OD_CNT_ARR_1003; \
        (co_config).ENTRY_H1003 = OD_ENTRY_H1003; \
        (co_config).CNT_SDO_SRV = OD_CNT_SDO_SRV; \
        (co_config).ENTRY_H1200 = OD_ENTRY_H1200; \
        (co_config).CNT_SYNC = OD_CNT_SYNC; \
        (co_config).ENTRY_H1005 = OD_ENTRY_H1005; \
        (co_config).ENTRY_H1006 = OD_ENTRY_H1006; \
        (co_config).ENTRY_H1007 = OD_ENTRY_H1007; \
        (co_config).ENTRY_H1019 = NULL; \
        (co_config).CNT_RPDO = OD_CNT_RPDO; \
        (co_config).ENTRY_H1400 = OD_ENTRY_H1400; \
        (co_config).ENTRY_H1600 = OD_ENTRY_H1600; \
        (co_config).CNT_TPDO = OD_CNT_TPDO; \
        (co_config).ENTRY_H1800 = OD_ENTRY_H1800; \
        (co_config).ENTRY_H1A00 = OD_ENTRY_H1A00; \
        (co_config).CNT_LEDS = OD_CNT_LEDS; \
        (co_config).CNT_LSS_SLV = OD_CNT_LSS_SLV; \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* OD_H */
