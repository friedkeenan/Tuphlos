#include <vector>
#include <filesystem>
namespace fs = std::filesystem;
#include <unordered_map>

#include <switch.h>

enum MTPOperationCode : u16 {
    OperationGetDeviceInfo = 0x1001,
    OperationOpenSession,
    OperationCloseSession,
    OperationGetStorageIds,
    OperationGetStorageInfo,
    OperationGetNumObjects,
    OperationGetObjectHandles,
    OperationGetObjectInfo,
    OperationGetObject,
    OperationGetThumb,
    OperationDeleteObject,
    OperationSendObjectInfo,
    OperationSendObject,
    OperationInitiateCapture,
    OperationFormatStore,
    OperationResetDevice,
    OperationSelfTest,
    OperationSetObjectProtection,
    OperationPowerDown,
    OperationGetDevicePropDesc,
    OperationGetDevicePropValue,
    OperationSetDevicePropValue,
    OperationResetDevicePropValue,
    OperationTerminateOpenCapture,
    OperationMoveObject,
    OperationCopyObject,
    OperationGetPartialObject,
    OperationInitiateOpenCapture,
    OperationGetObjectPropsSupported = 0x9801,
    OperationGetObjectPropDesc,
    OperationGetObjectPropValue,
    OperationSetObjectPropValue,
    OperationGetObjectReferences,
    OperationSetObjectReferences,
    OperationSkip = 0x9820,
};

enum MTPResponseCode : u16 {
    ResponseUndefined = 0x2000,
    ResponseOk,
    ResponseGeneralError,
    ResponseSessionNotOpen,
    ResponseInvalidTransactionId,
    ResponseOperationNotSupported,
    ResponseParamterNotSupported,
    ResponseIncompleteTransfer,
    ResponseInvalidStorageId,
    ResponseInvalidObjectHandle,
    ResponseDevicePropNotSupported,
    ResponseInvalidObjectFormatCode,
    ResponseStoreFull,
    ResponseObjectWriteProtected,
    ResponseStoreReadOnly,
    ResponseAccessDenied,
    ResponseNoThumbnailPresent,
    ResponseSelfTestFailed,
    ResponsePartialDeletion,
    ResponseStoreNotAvailable,
    ResponseSpecificationByFormatUnsupported,
    ResponseNovalidObjectInfo,
    ResponseInvalidCodeFormat,
    ResponseUnknownVendorCode,
    ResponseCaptureAlreadyTerminated,
    ResponseDeviceBusy,
    ResponseInvalidParentObject,
    ResponseInvalidDevicePropFormat,
    ResponseInvalidDevicePropValue,
    ResponseInvalidParameter,
    ResponseSessionAlreadyOpen,
    ResponseTransactionCancelled,
    ResponseSpecificatonOfDestinationUnsupported,
    ResponseInvalidObjectPropCode = 0xA801,
    ResponseInvalidObjectPropFormat,
    ResponseInvalidObjectPropValue,
    ResponseInvalidObjectReference,
    ResponseGroupNotSupported,
    ResponseInvalidDataset,
    ResponseSpecificationByGroupUnsupported,
    ResponseSpecificationByDepthUnsupported,
    ResponseObjectTooLarge,
    ResponseObjectPropNotSupported,
};

enum MTPEventCode : u16 {
    EventUndefined = 0x4000,
    EventCancelTransaction,
    EventObjectAdded,
    EventObjectRemoved,
    EventStoreAdded,
    EventStoreRemoved,
    EventDevicePropChanged,
    EventObjectInfoChanged,
    EventDeviceInfoChanged,
    EventRequestObjectTransfer,
    EventStoreFull,
    EventDeviceReset,
    EventStorageInfoChanged,
    EventCaptureComplete,
    EventUnreportedStatus,
    EventObjectPropChanged = 0xC801,
    EventObjectPropDescChanged,
    EventObjectReferencesChanged,
};

enum MTPDevicePropertyCode : u16 {
    PropertyUndefined = 0x5000,
    PropertyBatteryLevel,
    PropertyFunctionalMode,
    PropertyImageSize,
    PropertyCompressionSetting,
    PropertyWhiteBalance,
    PropertyRGBGain,
    PropertyFNumber,
    PropertyFocalLength,
    PropertyFocusDistance,
    PropertyFocusMode,
    PropertyExposureMeteringMode,
    PropertyFlashMode,
    PropertyExposureTime,
    PropertyExposureProgramMode,
    PropertyExposureIndex,
    PropertyExposureBiasCompensation,
    PropertyDateTime,
    PropertyCaptureDelay,
    PropertyStillCaptureMode,
    PropertyContrast,
    PropertySharpness,
    PropertyDigitalZoom,
    PropertyEffectMode,
    PropertBurstNumber,
    PropertyBurstInterval,
    PropertyTimelapseNumber,
    PropertyTimelapseInterval,
    PropertyFocusMeteringMode,
    PropertyUploadUrl,
    PropertyArtist,
    PropertyCopyrightInfo,
    PropertySynchronizationPartner = 0xD401,
    PropertyDeviceFriendlyName,
    PropertyVolume,
    PropertySupportedFormatsOrdered,
    PropertyDeviceIcon,
    PropertyPlaybackRate,
    PropertyPlaybackObject,
    PropertyPlaybackContainerIndex,
    PropertySessionInitiatorVersionInfo,
    PropertyPerceivedDeviceType,
};

enum MTPObjectFormatCode : u16 { // I would add all of them but I don't hate myself *that* much
    FormatUndefined = 0x3000,
    FormatAssociation,
};

enum MTPContainerType : u16 {
    ContainerTypeUndefined,
    ContainerTypeOperation,
    ContainerTypeData,
    ContainerTypeResponse,
    ContainerTypeEvent,
};

struct PACKED MTPContainerHeader {
    u32 length;
    u16 type;
    u16 code;
    u32 transaction_id;
};

class MTPResponse {
    public:
        MTPResponse(u16 code) : code(code) { };
        u16 code;
        u32 transaction_id;
        std::vector<u32> params;
};

class MTPOperation : public MTPResponse {
    public:
        MTPOperation(u16 code) : MTPResponse(code) { }
};

class MTPContainer {
    public:
        MTPContainer(MTPContainerHeader header);
        MTPContainer();
        ~MTPContainer();

        MTPContainerHeader header;
        u8 *data;

        void read(void *buffer, size_t size);
        void write(const void *buffer, size_t size);

        size_t read_cursor;

        u8 readU8();
        u16 readU16();
        u32 readU32();
        u64 readU64();
        std::u16string readString();

        void write(u8 var);
        void write(u16 var);
        void write(u32 var);
        void write(u64 var);
        void write(std::u16string var);
        template <class T> void write(std::vector<T> var);

        MTPOperation toOperation();
};

class MTPResponder {
    public:
        MTPResponder();
        ~MTPResponder();

        void loop();

        void insertStorage(const u32 id, std::string drive, std::u16string name);
    private:
        Result UsbXfer(UsbDsEndpoint *ep, size_t *out_xferd, void *buf, size_t size);
        u8 *read_buffer;
        size_t read_transferred;
        size_t read_cursor;
        u8 *write_buffer;
        Result read(void *buffer, size_t size);
        Result write(const void *buffer, size_t size);

        MTPContainer readContainer();
        Result writeContainer(MTPContainer &cont);

        MTPContainer createDataContainer(MTPOperation op);
        MTPResponse parseOperation(MTPOperation op);
        MTPContainer createResponseContainer(MTPResponse resp);

        u32 session_id;
        std::unordered_map<u32, std::pair<std::string, std::u16string>> storages;
        std::unordered_map<u32, fs::path> object_handles;

        u32 getObjectHandle(fs::path object);

        void GetDeviceInfo(MTPOperation op, MTPResponse *resp);
        void OpenSession(MTPOperation op, MTPResponse *resp);
        void CloseSession(MTPOperation op, MTPResponse *resp);
        void GetStorageIds(MTPOperation op, MTPResponse *resp);
        void GetStorageInfo(MTPOperation op, MTPResponse *resp);
        void GetObjectHandles(MTPOperation op, MTPResponse *resp);
        void GetObjectInfo(MTPOperation op, MTPResponse *resp);
};