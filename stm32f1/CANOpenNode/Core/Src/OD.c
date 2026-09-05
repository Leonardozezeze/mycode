/*
 * Object Dictionary implementation for CANopenNode
 *
 * This file contains the actual Object Dictionary data and initialization.
 * Generated manually based on CANopenNode requirements.
 */

#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "OD.h"

/*
 * OD variable instances - actual data storage
 */

/* Persistent communication parameters (stored in non-volatile memory) */
OD_PERSIST_COMM_t OD_PERSIST_COMM = {
    .x1000_deviceType = 0x000F0191,  /* Example: device type */
    .x1018_identity = {
        .maxSubIndex = 4,
        .vendor_ID = 0x00000000,
        .productCode = 0x00000000,
        .revisionNumber = 0x00010000,  /* Major.Minor */
        .serialNumber = 0x00000001
    },
    .x1005_syncCOBID = 0x00000080,  /* COB-ID SYNC = 0x80 */
    .x1006_communicationCyclePeriod = 0,  /* Not used */
    .x1007_synchronousWindowLength = 0,
    .x1017_producerHeartbeatTime = 1000,  /* 1000ms = 1s */
    .x1014_cobIDEmergency = 0x00000080,  /* default EMCY base 0x80; stack adds nodeId(42) -> 0xAA */
    .x1015_inhibitTimeEmergency = 100,  /* 100ms */
    .x1016_consumerHeartbeatTime = {
        .maxSubIndex = 1,
        .consumerHeartbeatTime = {0}
    },
    .x1010_storeParameters = {
        .maxSubIndex = 1,
        .parameter = {0}
    },
    .x1011_restoreDefaultParameters = {
        .maxSubIndex = 1,
        .parameter = {0}
    },
    .x1800_tpdo1Param = {
        .maxSubIndex = 6,
        .COBID = 0x800001AA,   /* TPDO1: 0x180 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,  /* Event-driven */
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    },
    .x1801_tpdo2Param = {
        .maxSubIndex = 6,
        .COBID = 0x800002AA,   /* TPDO2: 0x280 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    },
    .x1802_tpdo3Param = {
        .maxSubIndex = 6,
        .COBID = 0x800003AA,   /* TPDO3: 0x380 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    },
    .x1803_tpdo4Param = {
        .maxSubIndex = 6,
        .COBID = 0x800004AA,   /* TPDO4: 0x480 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    },
    .x1400_rpdo1Param = {
        .maxSubIndex = 5,
        .COBID = 0x8000022A,   /* RPDO1: 0x200 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    },
    .x1401_rpdo2Param = {
        .maxSubIndex = 5,
        .COBID = 0x8000032A,   /* RPDO2: 0x300 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    },
    .x1402_rpdo3Param = {
        .maxSubIndex = 5,
        .COBID = 0x8000042A,   /* RPDO3: 0x400 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    },
    .x1403_rpdo4Param = {
        .maxSubIndex = 5,
        .COBID = 0x8000052A,   /* RPDO4: 0x500 + nodeId 42, bit31=1 禁用(无映射) */
        .transmissionType = 254,
        .inhibitTime = 0,
        .eventTimer = 0,
        .SYNCstartValue = 0
    }
};

/* RAM variables (runtime, not stored) */
OD_RAM_t OD_RAM = {
    .x1001_errorRegister = 0,
    .x1003_preDefinedErrorField_sub0 = 0,
    .x1003_preDefinedErrorField = {0, 0, 0, 0, 0, 0, 0, 0},
    .x1002_manufacturerStatusRegister = 0,
    .x1A00_tpdo1Mapping = {0},
    .x1A01_tpdo2Mapping = {0},
    .x1A02_tpdo3Mapping = {0},
    .x1A03_tpdo4Mapping = {0},
    .x1600_rpdo1Mapping = {0},
    .x1601_rpdo2Mapping = {0},
    .x1602_rpdo3Mapping = {0},
    .x1603_rpdo4Mapping = {0},
    .x2000_applicationVariable1 = 0,
    .x2001_applicationVariable2 = 0,
    .x2002_applicationVariable3 = 0
};

/*
 * OD objects - describe each OD entry
 */

typedef struct {
    OD_obj_var_t o_1000_deviceType;
    OD_obj_var_t o_1001_errorRegister;
    OD_obj_var_t o_1002_manufacturerStatusRegister;
    OD_obj_array_t o_1003_preDefinedErrorField;
    OD_obj_var_t o_1005_syncCOBID;
    OD_obj_var_t o_1006_communicationCyclePeriod;
    OD_obj_var_t o_1007_synchronousWindowLength;
    OD_obj_var_t o_1014_cobIDEmergency;
    OD_obj_var_t o_1015_inhibitTimeEmergency;
    OD_obj_record_t o_1016_consumerHeartbeatTime[2];
    OD_obj_var_t o_1017_producerHeartbeatTime;
    OD_obj_record_t o_1018_identity[5];
    OD_obj_record_t o_1200_sdoServerParam[3];
    OD_obj_record_t o_1400_rpdo1Param[6];
    OD_obj_record_t o_1401_rpdo2Param[6];
    OD_obj_record_t o_1402_rpdo3Param[6];
    OD_obj_record_t o_1403_rpdo4Param[6];
    OD_obj_record_t o_1600_rpdo1Mapping[5];
    OD_obj_record_t o_1601_rpdo2Mapping[5];
    OD_obj_record_t o_1602_rpdo3Mapping[5];
    OD_obj_record_t o_1603_rpdo4Mapping[5];
    OD_obj_record_t o_1800_tpdo1Param[6];
    OD_obj_record_t o_1801_tpdo2Param[6];
    OD_obj_record_t o_1802_tpdo3Param[6];
    OD_obj_record_t o_1803_tpdo4Param[6];
    OD_obj_record_t o_1A00_tpdo1Mapping[5];
    OD_obj_record_t o_1A01_tpdo2Mapping[5];
    OD_obj_record_t o_1A02_tpdo3Mapping[5];
    OD_obj_record_t o_1A03_tpdo4Mapping[5];
    OD_obj_record_t o_1010_storeParameters[2];
    OD_obj_record_t o_1011_restoreDefaultParameters[2];
} OD_Objs_t;

static CO_PROGMEM OD_Objs_t OD_Objs = {
    /* 0x1000 Device Type */
    .o_1000_deviceType = {
        .dataOrig = &OD_PERSIST_COMM.x1000_deviceType,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4
    },
    /* 0x1001 Error Register */
    .o_1001_errorRegister = {
        .dataOrig = &OD_RAM.x1001_errorRegister,
        .attribute = ODA_SDO_R | ODA_TPDO,
        .dataLength = 1
    },
    /* 0x1002 Manufacturer Status Register */
    .o_1002_manufacturerStatusRegister = {
        .dataOrig = &OD_RAM.x1002_manufacturerStatusRegister,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4
    },
    /* 0x1003 Pre-defined Error Field */
    .o_1003_preDefinedErrorField = {
        .dataOrig0 = &OD_RAM.x1003_preDefinedErrorField_sub0,
        .dataOrig = &OD_RAM.x1003_preDefinedErrorField[0],
        .attribute0 = ODA_SDO_RW,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    /* 0x1005 COB-ID SYNC */
    .o_1005_syncCOBID = {
        .dataOrig = &OD_PERSIST_COMM.x1005_syncCOBID,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1006 Communication Cycle Period */
    .o_1006_communicationCyclePeriod = {
        .dataOrig = &OD_PERSIST_COMM.x1006_communicationCyclePeriod,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1007 Synchronous Window Length */
    .o_1007_synchronousWindowLength = {
        .dataOrig = &OD_PERSIST_COMM.x1007_synchronousWindowLength,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1014 COB-ID Emergency */
    .o_1014_cobIDEmergency = {
        .dataOrig = &OD_PERSIST_COMM.x1014_cobIDEmergency,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1015 Inhibit Time Emergency */
    .o_1015_inhibitTimeEmergency = {
        .dataOrig = &OD_PERSIST_COMM.x1015_inhibitTimeEmergency,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 2
    },
    /* 0x1016 Consumer Heartbeat Time */
    .o_1016_consumerHeartbeatTime = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1016_consumerHeartbeatTime.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1016_consumerHeartbeatTime.consumerHeartbeatTime[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1017 Producer Heartbeat Time */
    .o_1017_producerHeartbeatTime = {
        .dataOrig = &OD_PERSIST_COMM.x1017_producerHeartbeatTime,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 2
    },
    /* 0x1018 Identity Object */
    .o_1018_identity = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1018_identity.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1018_identity.vendor_ID,
            .subIndex = 1,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1018_identity.productCode,
            .subIndex = 2,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1018_identity.revisionNumber,
            .subIndex = 3,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1018_identity.serialNumber,
            .subIndex = 4,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1200 SDO Server Parameter */
    .o_1200_sdoServerParam = {
        {
            .dataOrig = NULL,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = NULL,
            .subIndex = 1,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = NULL,
            .subIndex = 2,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1400 RPDO1 Parameter */
    .o_1400_rpdo1Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1400_rpdo1Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1400_rpdo1Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1400_rpdo1Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = NULL,
            .subIndex = 3,
            .attribute = 0,
            .dataLength = 0
        },
        {
            .dataOrig = NULL,
            .subIndex = 4,
            .attribute = 0,
            .dataLength = 0
        },
        {
            .dataOrig = NULL,
            .subIndex = 5,
            .attribute = 0,
            .dataLength = 0
        }
    },
    /* 0x1401 RPDO2 Parameter */
    .o_1401_rpdo2Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1401_rpdo2Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1401_rpdo2Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1401_rpdo2Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = NULL, .subIndex = 3, .attribute = 0, .dataLength = 0
        },
        {
            .dataOrig = NULL, .subIndex = 4, .attribute = 0, .dataLength = 0
        },
        {
            .dataOrig = NULL, .subIndex = 5, .attribute = 0, .dataLength = 0
        }
    },
    /* 0x1402 RPDO3 Parameter */
    .o_1402_rpdo3Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1402_rpdo3Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1402_rpdo3Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1402_rpdo3Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = NULL, .subIndex = 3, .attribute = 0, .dataLength = 0
        },
        {
            .dataOrig = NULL, .subIndex = 4, .attribute = 0, .dataLength = 0
        },
        {
            .dataOrig = NULL, .subIndex = 5, .attribute = 0, .dataLength = 0
        }
    },
    /* 0x1403 RPDO4 Parameter */
    .o_1403_rpdo4Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1403_rpdo4Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1403_rpdo4Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1403_rpdo4Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = NULL, .subIndex = 3, .attribute = 0, .dataLength = 0
        },
        {
            .dataOrig = NULL, .subIndex = 4, .attribute = 0, .dataLength = 0
        },
        {
            .dataOrig = NULL, .subIndex = 5, .attribute = 0, .dataLength = 0
        }
    },
    /* 0x1600 RPDO1 Mapping */
    .o_1600_rpdo1Mapping = {
        {
            .dataOrig = &OD_RAM.x1600_rpdo1Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1600_rpdo1Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1600_rpdo1Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1600_rpdo1Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1600_rpdo1Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1601 RPDO2 Mapping */
    .o_1601_rpdo2Mapping = {
        {
            .dataOrig = &OD_RAM.x1601_rpdo2Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1601_rpdo2Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1601_rpdo2Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1601_rpdo2Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1601_rpdo2Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1602 RPDO3 Mapping */
    .o_1602_rpdo3Mapping = {
        {
            .dataOrig = &OD_RAM.x1602_rpdo3Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1602_rpdo3Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1602_rpdo3Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1602_rpdo3Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1602_rpdo3Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1603 RPDO4 Mapping */
    .o_1603_rpdo4Mapping = {
        {
            .dataOrig = &OD_RAM.x1603_rpdo4Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1603_rpdo4Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1603_rpdo4Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1603_rpdo4Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1603_rpdo4Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1800 TPDO1 Parameter */
    .o_1800_tpdo1Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1800_tpdo1Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1800_tpdo1Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1800_tpdo1Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1800_tpdo1Param.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1800_tpdo1Param.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1800_tpdo1Param.SYNCstartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    /* 0x1801 TPDO2 Parameter */
    .o_1801_tpdo2Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1801_tpdo2Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1801_tpdo2Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1801_tpdo2Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1801_tpdo2Param.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1801_tpdo2Param.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1801_tpdo2Param.SYNCstartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    /* 0x1802 TPDO3 Parameter */
    .o_1802_tpdo3Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1802_tpdo3Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1802_tpdo3Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1802_tpdo3Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1802_tpdo3Param.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1802_tpdo3Param.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1802_tpdo3Param.SYNCstartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    /* 0x1803 TPDO4 Parameter */
    .o_1803_tpdo4Param = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1803_tpdo4Param.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1803_tpdo4Param.COBID,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1803_tpdo4Param.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1803_tpdo4Param.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1803_tpdo4Param.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1803_tpdo4Param.SYNCstartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    /* 0x1A00 TPDO1 Mapping */
    .o_1A00_tpdo1Mapping = {
        {
            .dataOrig = &OD_RAM.x1A00_tpdo1Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1A00_tpdo1Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A00_tpdo1Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A00_tpdo1Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A00_tpdo1Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1A01 TPDO2 Mapping */
    .o_1A01_tpdo2Mapping = {
        {
            .dataOrig = &OD_RAM.x1A01_tpdo2Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1A01_tpdo2Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A01_tpdo2Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A01_tpdo2Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A01_tpdo2Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1A02 TPDO3 Mapping */
    .o_1A02_tpdo3Mapping = {
        {
            .dataOrig = &OD_RAM.x1A02_tpdo3Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1A02_tpdo3Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A02_tpdo3Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A02_tpdo3Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A02_tpdo3Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1A03 TPDO4 Mapping */
    .o_1A03_tpdo4Mapping = {
        {
            .dataOrig = &OD_RAM.x1A03_tpdo4Mapping.numberOfMappedObjects,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_RAM.x1A03_tpdo4Mapping.mappedObject[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A03_tpdo4Mapping.mappedObject[1],
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A03_tpdo4Mapping.mappedObject[2],
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .dataOrig = &OD_RAM.x1A03_tpdo4Mapping.mappedObject[3],
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1010 Store Parameters */
    .o_1010_storeParameters = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1010_storeParameters.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1010_storeParameters.parameter[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    /* 0x1011 Restore Default Parameters */
    .o_1011_restoreDefaultParameters = {
        {
            .dataOrig = &OD_PERSIST_COMM.x1011_restoreDefaultParameters.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .dataOrig = &OD_PERSIST_COMM.x1011_restoreDefaultParameters.parameter[0],
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    }
};

/*
 * Object Dictionary - list of all OD entries
 *
 * NOTE: must be writable (RAM). CANopenNode writes the `extension` pointer of
 * entries at runtime (OD_extension_init), so `const` would place it in flash
 * and a write would cause a (imprecise) bus fault -> HardFault.
 */
static OD_entry_t OD_List[] = {
    {0x1000, 0x01, ODT_VAR, &OD_Objs.o_1000_deviceType, NULL},
    {0x1001, 0x01, ODT_VAR, &OD_Objs.o_1001_errorRegister, NULL},
    {0x1002, 0x01, ODT_VAR, &OD_Objs.o_1002_manufacturerStatusRegister, NULL},
    {0x1003, 0x09, ODT_ARR, &OD_Objs.o_1003_preDefinedErrorField, NULL},
    {0x1005, 0x01, ODT_VAR, &OD_Objs.o_1005_syncCOBID, NULL},
    {0x1006, 0x01, ODT_VAR, &OD_Objs.o_1006_communicationCyclePeriod, NULL},
    {0x1007, 0x01, ODT_VAR, &OD_Objs.o_1007_synchronousWindowLength, NULL},
    {0x1014, 0x01, ODT_VAR, &OD_Objs.o_1014_cobIDEmergency, NULL},
    {0x1015, 0x01, ODT_VAR, &OD_Objs.o_1015_inhibitTimeEmergency, NULL},
    {0x1016, 0x02, ODT_REC, &OD_Objs.o_1016_consumerHeartbeatTime, NULL},
    {0x1017, 0x01, ODT_VAR, &OD_Objs.o_1017_producerHeartbeatTime, NULL},
    {0x1018, 0x05, ODT_REC, &OD_Objs.o_1018_identity, NULL},
    {0x1200, 0x03, ODT_REC, &OD_Objs.o_1200_sdoServerParam, NULL},
    {0x1400, 0x06, ODT_REC, &OD_Objs.o_1400_rpdo1Param, NULL},
    {0x1401, 0x06, ODT_REC, &OD_Objs.o_1401_rpdo2Param, NULL},
    {0x1402, 0x06, ODT_REC, &OD_Objs.o_1402_rpdo3Param, NULL},
    {0x1403, 0x06, ODT_REC, &OD_Objs.o_1403_rpdo4Param, NULL},
    {0x1600, 0x05, ODT_REC, &OD_Objs.o_1600_rpdo1Mapping, NULL},
    {0x1601, 0x05, ODT_REC, &OD_Objs.o_1601_rpdo2Mapping, NULL},
    {0x1602, 0x05, ODT_REC, &OD_Objs.o_1602_rpdo3Mapping, NULL},
    {0x1603, 0x05, ODT_REC, &OD_Objs.o_1603_rpdo4Mapping, NULL},
    {0x1800, 0x06, ODT_REC, &OD_Objs.o_1800_tpdo1Param, NULL},
    {0x1801, 0x06, ODT_REC, &OD_Objs.o_1801_tpdo2Param, NULL},
    {0x1802, 0x06, ODT_REC, &OD_Objs.o_1802_tpdo3Param, NULL},
    {0x1803, 0x06, ODT_REC, &OD_Objs.o_1803_tpdo4Param, NULL},
    {0x1A00, 0x05, ODT_REC, &OD_Objs.o_1A00_tpdo1Mapping, NULL},
    {0x1A01, 0x05, ODT_REC, &OD_Objs.o_1A01_tpdo2Mapping, NULL},
    {0x1A02, 0x05, ODT_REC, &OD_Objs.o_1A02_tpdo3Mapping, NULL},
    {0x1A03, 0x05, ODT_REC, &OD_Objs.o_1A03_tpdo4Mapping, NULL},
    {0x1010, 0x02, ODT_REC, &OD_Objs.o_1010_storeParameters, NULL},
    {0x1011, 0x02, ODT_REC, &OD_Objs.o_1011_restoreDefaultParameters, NULL},
    {0x0000, 0x00, 0, NULL, NULL}  /* End marker */
};

/*
 * Global OD instance
 */
static OD_t _OD = {
    .size = (sizeof(OD_List) / sizeof(OD_List[0])) - 1,
    .list = &OD_List[0]
};

OD_t *OD = &_OD;
