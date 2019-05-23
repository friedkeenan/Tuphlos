#include "mtp.hpp"

#include <cstring>
#include <ctime>
#include <iomanip>

#include <malloc.h>
#include <stdio.h>

#include <dirent.h>

#define BUF_SIZE 0x400

UsbDsInterface *g_interface;
UsbDsEndpoint *g_endpoint_in, *g_endpoint_out, *g_endpoint_interr;

bool g_initialized = false;

/* Lots of low level USB stuff taken from libnx and Atmosphere's tma_usb_comms */

static Result _usbCommsInterfaceInit1x() {
    Result rc = 0;

    u8 mtp_index;
    usbDsAddUsbStringDescriptor(&mtp_index, "MTP");

    struct usb_interface_descriptor interface_descriptor = {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 4,
        .bNumEndpoints = 3,
        .bInterfaceClass = 6,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 1,
        .iInterface = mtp_index,
    };

    struct usb_endpoint_descriptor endpoint_descriptor_in = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_IN,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x200,
    };

    struct usb_endpoint_descriptor endpoint_descriptor_out = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_OUT,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x200,
    };

    struct usb_endpoint_descriptor endpoint_descriptor_interr = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_IN,
        .bmAttributes = USB_TRANSFER_TYPE_INTERRUPT,
        .wMaxPacketSize = 0x1c,
        .bInterval = 6,
    };

    if (R_FAILED(rc)) return rc;

    //Setup interface.
    rc = usbDsGetDsInterface(&g_interface, &interface_descriptor, "usb");
    if (R_FAILED(rc)) return rc;

    //Setup endpoints.
    rc = usbDsInterface_GetDsEndpoint(g_interface, &g_endpoint_in, &endpoint_descriptor_in);//device->host
    if (R_FAILED(rc)) return rc;

    rc = usbDsInterface_GetDsEndpoint(g_interface, &g_endpoint_out, &endpoint_descriptor_out);//host->device
    if (R_FAILED(rc)) return rc;

    rc = usbDsInterface_GetDsEndpoint(g_interface, &g_endpoint_interr, &endpoint_descriptor_interr);

    return rc;
}

static Result _usbCommsInterfaceInit5x() {
    Result rc = 0;
    
    u8 iManufacturer, iProduct, iSerialNumber;
    static const u16 supported_langs[1] = {0x0409};
    // Send language descriptor
    rc = usbDsAddUsbLanguageStringDescriptor(NULL, supported_langs, sizeof(supported_langs)/sizeof(u16));
    // Send manufacturer
    if (R_SUCCEEDED(rc)) rc = usbDsAddUsbStringDescriptor(&iManufacturer, "Nintendo");
    // Send product
    if (R_SUCCEEDED(rc)) rc = usbDsAddUsbStringDescriptor(&iProduct, "Nintendo Switch");
    // Send serial number
    if (R_SUCCEEDED(rc)) rc = usbDsAddUsbStringDescriptor(&iSerialNumber, "SerialNumber");

    // Send device descriptors
    struct usb_device_descriptor device_descriptor = {
        .bLength = USB_DT_DEVICE_SIZE,
        .bDescriptorType = USB_DT_DEVICE,
        .bcdUSB = 0x0110,
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 0x40,
        .idVendor = 0x057e,
        .idProduct = 0x3000,
        .bcdDevice = 0x0100,
        .iManufacturer = iManufacturer,
        .iProduct = iProduct,
        .iSerialNumber = iSerialNumber,
        .bNumConfigurations = 0x01
    };
    // Full Speed is USB 1.1
    if (R_SUCCEEDED(rc)) rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Full, &device_descriptor);
    
    // High Speed is USB 2.0
    device_descriptor.bcdUSB = 0x0200;
    if (R_SUCCEEDED(rc)) rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_High, &device_descriptor);
    
    // Super Speed is USB 3.0
    device_descriptor.bcdUSB = 0x0300;
    // Upgrade packet size to 512
    device_descriptor.bMaxPacketSize0 = 0x09;
    if (R_SUCCEEDED(rc)) rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Super, &device_descriptor);
    
    // Define Binary Object Store
    u8 bos[0x16] = {
        0x05, // .bLength
        USB_DT_BOS, // .bDescriptorType
        0x16, 0x00, // .wTotalLength
        0x02, // .bNumDeviceCaps
        
        // USB 2.0
        0x07, // .bLength
        USB_DT_DEVICE_CAPABILITY, // .bDescriptorType
        0x02, // .bDevCapabilityType
        0x02, 0x00, 0x00, 0x00, // dev_capability_data
        
        // USB 3.0
        0x0A, // .bLength
        USB_DT_DEVICE_CAPABILITY, // .bDescriptorType
        0x03, // .bDevCapabilityType
        0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00
    };
    if (R_SUCCEEDED(rc)) rc = usbDsSetBinaryObjectStore(bos, sizeof(bos));
    
    if (R_FAILED(rc)) return rc;

    u8 mtp_index;
    usbDsAddUsbStringDescriptor(&mtp_index, "MTP");
    
    struct usb_interface_descriptor interface_descriptor = {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 4,
        .bNumEndpoints = 3,
        .bInterfaceClass = 6,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 1,
        .iInterface = mtp_index,
    };

    struct usb_endpoint_descriptor endpoint_descriptor_in = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_IN,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x40,
    };

    struct usb_endpoint_descriptor endpoint_descriptor_out = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_OUT,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x40,
    };

    struct usb_endpoint_descriptor endpoint_descriptor_interr = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_IN,
        .bmAttributes = USB_TRANSFER_TYPE_INTERRUPT,
        .wMaxPacketSize = 0x1c,
        .bInterval = 6,
    };
    
    struct usb_ss_endpoint_companion_descriptor endpoint_companion = {
        .bLength = sizeof(struct usb_ss_endpoint_companion_descriptor),
        .bDescriptorType = USB_DT_SS_ENDPOINT_COMPANION,
        .bMaxBurst = 0x0F,
        .bmAttributes = 0x00,
        .wBytesPerInterval = 0x00,
    };
    
    rc = usbDsRegisterInterface(&g_interface);
    if (R_FAILED(rc)) return rc;
    
    interface_descriptor.bInterfaceNumber = g_interface->interface_index;
    endpoint_descriptor_in.bEndpointAddress += interface_descriptor.bInterfaceNumber + 1;
    endpoint_descriptor_out.bEndpointAddress += interface_descriptor.bInterfaceNumber + 1;
    endpoint_descriptor_interr.bEndpointAddress += interface_descriptor.bInterfaceNumber +2;
    
    // Full Speed Config
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Full, &interface_descriptor, USB_DT_INTERFACE_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Full, &endpoint_descriptor_in, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Full, &endpoint_descriptor_out, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Full, &endpoint_descriptor_interr, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    
    // High Speed Config
    endpoint_descriptor_in.wMaxPacketSize = 0x200;
    endpoint_descriptor_out.wMaxPacketSize = 0x200;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_High, &interface_descriptor, USB_DT_INTERFACE_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_High, &endpoint_descriptor_in, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_High, &endpoint_descriptor_out, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_High, &endpoint_descriptor_interr, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    
    // Super Speed Config
    endpoint_descriptor_in.wMaxPacketSize = 0x400;
    endpoint_descriptor_out.wMaxPacketSize = 0x400;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Super, &interface_descriptor, USB_DT_INTERFACE_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Super, &endpoint_descriptor_in, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Super, &endpoint_companion, USB_DT_SS_ENDPOINT_COMPANION_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Super, &endpoint_descriptor_out, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Super, &endpoint_companion, USB_DT_SS_ENDPOINT_COMPANION_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Super, &endpoint_descriptor_interr, USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) return rc;
    rc = usbDsInterface_AppendConfigurationData(g_interface, UsbDeviceSpeed_Super, &endpoint_companion, USB_DT_SS_ENDPOINT_COMPANION_SIZE);
    if (R_FAILED(rc)) return rc;
    
    //Setup endpoints.    
    rc = usbDsInterface_RegisterEndpoint(g_interface, &g_endpoint_in, endpoint_descriptor_in.bEndpointAddress);
    if (R_FAILED(rc)) return rc;
    
    rc = usbDsInterface_RegisterEndpoint(g_interface, &g_endpoint_out, endpoint_descriptor_out.bEndpointAddress);
    if (R_FAILED(rc)) return rc;

    rc = usbDsInterface_RegisterEndpoint(g_interface, &g_endpoint_interr, endpoint_descriptor_interr.bEndpointAddress);
    if (R_FAILED(rc)) return rc;
    
    return rc;
}

static Result _usbCommsInterfaceInit() {
    if (hosversionAtLeast(5,0,0)) {
        return _usbCommsInterfaceInit5x();
    } else {
        return _usbCommsInterfaceInit1x();
    }
}

static Result _usbCommsInitialize() {
    Result rc = 0;

    if (g_initialized)
        return rc;

    rc = usbDsInitialize();
    if (R_FAILED(rc))
        return rc;

    rc = _usbCommsInterfaceInit();
    if (R_FAILED(rc))
        return rc;

    rc = usbDsInterface_EnableInterface(g_interface);
    if (R_FAILED(rc))
        return rc;

    rc = usbDsEnable();
    if (R_FAILED(rc))
        return rc;

    g_initialized = true;

    return rc;
}

static void _usbCommsExit() {
    usbDsExit();
    g_initialized = false;
}

MTPResponse::MTPResponse(MTPResponseCode code) {
    this->code = code;
}

MTPOperation::MTPOperation(MTPOperationCode code) {
    this->code = code;
}

MTPContainer::MTPContainer(MTPContainerHeader header) {
    this->header = header;
    this->data = NULL;
    this->read_cursor = 0;
}

MTPContainer::MTPContainer() {
    this->header = {
        .length = sizeof(MTPContainer),
        .type = ContainerTypeUndefined,
        .code = 0xFFFF,
        .transaction_id = 0
    };
    this->data = NULL;
    this->read_cursor = 0;
}


MTPContainer::~MTPContainer() {
    free(this->data);
}

void MTPContainer::read(void *buffer, size_t size) {
    if (this->data == NULL || this->read_cursor + size > this->header.length - sizeof(MTPContainerHeader))
        return;

    memcpy(buffer, this->data + this->read_cursor, size);
    this->read_cursor += size;
}

void MTPContainer::write(const void *buffer, size_t size) {
    printf("WRITE SIZE: 0x%lx\n", size);
    this->data = (u8 *) realloc(this->data, this->header.length - sizeof(MTPContainerHeader) + size);
    memcpy(this->data + this->header.length - sizeof(MTPContainerHeader), buffer, size);
    this->header.length += size;
}

u8 MTPContainer::readU8() {
    u8 var;
    this->read(&var, sizeof(var));
    return var;
}

u16 MTPContainer::readU16() {
    u16 var;
    this->read(&var, sizeof(var));
    return var;
}

u32 MTPContainer::readU32() {
    u32 var;
    this->read(&var, sizeof(var));
    return var;
}

u64 MTPContainer::readU64() {
    u64 var;
    this->read(&var, sizeof(var));
    return var;
}

std::u16string MTPContainer::readString() {
    u8 length = this->readU8();
    char16_t c_str[length];
    this->read(c_str, length);
    std::u16string var = c_str;
    return var;
}

void MTPContainer::write(u8 var) {
    this->write(&var, sizeof(var));
}

void MTPContainer::write(u16 var) {
    this->write(&var, sizeof(var));
}

void MTPContainer::write(u32 var) {
    this->write(&var, sizeof(var));
}

void MTPContainer::write(u64 var) {
    this->write(&var, sizeof(var));
}

void MTPContainer::write(std::u16string var) {
    if (var.empty()) {
        this->write((u8) 0);
    } else {
        this->write((u8) (var.length() + 1));
        this->write(var.c_str(), var.length() + 1);
    }
}

template <class T> void MTPContainer::write(std::vector<T> var) {
    u32 length = var.size();
    this->write(length);
    for (u32 i=0; i<length; i++) {
        this->write(var[i]);
    }
}

MTPOperation MTPContainer::toOperation() {
    MTPOperation op(OperationSkip);

    if (this->header.type == ContainerTypeOperation) {
        op.code = this->header.code;
        op.transaction_id = this->header.transaction_id;
        for(size_t i=0; i < 5*sizeof(u32) && i < this->header.length - sizeof(this->header); i+=sizeof(u32)) {
            u32 param = this->readU32();
            printf("PARAM: 0x%x\n", param);
            op.params.push_back(param);
        }
    }

    return op;
}

MTPResponder::MTPResponder() {
    _usbCommsInitialize();
    usbDsWaitReady(U64_MAX);

    this->read_buffer = (u8 *) memalign(0x1000, BUF_SIZE);
    this->write_buffer = (u8 *) memalign(0x1000, BUF_SIZE);
    this->read_cursor = 0;
    this->read_transferred = 0;

    this->session_id = 0;
}

MTPResponder::~MTPResponder() {
    _usbCommsExit();
}

Result MTPResponder::loop() {
    printf("READ CONTAINER\n");
    MTPContainer cont = this->readContainer();
    printf("READ CONTAINER DONE\n");

    printf("TO OPERATION\n");
    MTPOperation op = cont.toOperation();
    printf("OPERATION: 0x%x %ld\n", op.code, op.params.size());
    MTPResponse resp = this->parseOperation(op);
    printf("AFTER PARSE OPERATION\n");
    printf("RESPONSE: 0x%x %ld\n", resp.code, resp.params.size());
    cont = this->createResponseContainer(resp);

    printf("BEFORE WRITE CONTAINER\n");
    this->writeContainer(cont);
    printf("AFTER WRITE CONTAINER\n");
    return 0;
}

void MTPResponder::insertStorage(const u32 id, const std::string drive, const std::u16string name) {
    this->storages[id] = std::pair<std::string, std::u16string>(drive, name);
}

/* Taken from Atmosphere's tma_usb_comms */
Result MTPResponder::UsbXfer(UsbDsEndpoint *ep, size_t *out_xferd, void *buf, size_t size) {
    Result rc = 0;
    u32 urbId = 0;
    u32 total_xferd = 0;
    UsbDsReportData reportdata;
    
    if (size) {
        /* Start transfer. */
        rc = usbDsEndpoint_PostBufferAsync(ep, buf, size, &urbId);
        if (R_FAILED(rc)) return rc;
        
        /* Wait for transfer to complete. */
        eventWait(&ep->CompletionEvent, U64_MAX);
        eventClear(&ep->CompletionEvent);
        
        rc = usbDsEndpoint_GetReportData(ep, &reportdata);
        if (R_FAILED(rc)) return rc;

        rc = usbDsParseReportData(&reportdata, urbId, NULL, &total_xferd);
        if (R_FAILED(rc)) return rc;   
    }
    
    if (out_xferd) *out_xferd = total_xferd;
    
    return rc;
}

Result MTPResponder::read(void *buffer, size_t size) {
    Result rc = 0;

    if (size <= 0)
        return rc;

    if (this->read_cursor >= this->read_transferred) {
        this->read_transferred = 0;
        this->read_cursor = 0;
        rc = UsbXfer(g_endpoint_out, &this->read_transferred, this->read_buffer, 0x400 /*size*/);
        if (R_FAILED(rc))
            return rc;
    }

    memcpy(buffer, this->read_buffer+this->read_cursor, size);
    this->read_cursor += size;

    return rc;
}

Result MTPResponder::write(const void *buffer, size_t size) {
    Result rc = 0;

    if (size <= 0)
        return rc;

    memcpy(this->write_buffer, buffer, size);

    UsbXfer(g_endpoint_in, NULL, this->write_buffer, size);

    return rc;
}

MTPContainer MTPResponder::readContainer() {
    MTPContainerHeader header;

    this->read(&header, sizeof(header));
    /*if (R_FAILED(rc))
        return rc;*/

    MTPContainer cont(header);
    cont.data = (u8 *) malloc(cont.header.length - sizeof(MTPContainerHeader));
    this->read(cont.data, cont.header.length - sizeof(MTPContainerHeader));

    return cont;
}

Result MTPResponder::writeContainer(MTPContainer cont) {
    Result rc = this->write(&cont.header, sizeof(cont.header));
    if (R_FAILED(rc))
        return rc;
    rc = this->write(cont.data, cont.header.length - sizeof(cont.header));

    return rc;
}

u32 MTPResponder::getObjectHandle(fs::path object) {
    printf("GETTING HANDLE\n");
    u32 handle;
    for(handle=1; handle <= this->object_handles.size(); handle++) { // Object handle of zero is reserved
        if (this->object_handles[handle] == object) {
            printf("BREAK\n");
            break;
        }
    }

    printf("HANDLE: 0x%x\n", handle);

    if (handle == this->object_handles.size() + 1) {
        printf("INSERT\n");
        this->object_handles.insert({handle, object});
        printf("DONE INSERT\n");
    }

    printf("RETURN\n");
    return handle;
}

MTPResponse MTPResponder::parseOperation(MTPOperation op) {
    MTPResponse resp(ResponseOperationNotSupported);
    resp.transaction_id = op.transaction_id;

    switch(op.code) {
        case OperationGetDeviceInfo:
            this->GetDeviceInfo(op, &resp);
            break;
        case OperationOpenSession:
            this->OpenSession(op, &resp);
            break;
        case OperationCloseSession:
            this->CloseSession(op, &resp);
            break;
        case OperationGetStorageIds:
            this->GetStorageIds(op, &resp);
            break;
        case OperationGetStorageInfo:
            this->GetStorageInfo(op, &resp);
            break;
        case OperationGetObjectHandles:
            this->GetObjectHandles(op, &resp);
            break;
        case OperationGetObjectInfo:
            this->GetObjectInfo(op, &resp);
            break;
    }

    printf("BEFORE RET RESP\n");
    return resp;
}

MTPContainer MTPResponder::createDataContainer(MTPOperation op) {
    MTPContainerHeader header;
    header.length = sizeof(MTPContainerHeader);
    header.type = ContainerTypeData;
    header.code = op.code;
    header.transaction_id = op.transaction_id;

    MTPContainer cont(header);
    return cont;
}

MTPContainer MTPResponder::createResponseContainer(MTPResponse resp) {
    MTPContainerHeader header;
    header.length = sizeof(MTPContainerHeader);
    header.type = ContainerTypeResponse;
    header.code = resp.code;
    header.transaction_id = resp.transaction_id;

    MTPContainer cont(header);

    for (auto param : resp.params) {
        cont.write(param);
    }

    return cont;
}

void MTPResponder::GetDeviceInfo(MTPOperation op, MTPResponse *resp) {
    MTPContainer cont = this->createDataContainer(op);
    cont.write((u16) 100); // Standard Version
    cont.write((u32) 0xFFFFFFFF); // Vendor Extension ID
    cont.write((u16) 100); // MTP Version
    cont.write(u"microsoft.com: 1.0;"); // Extensions
    cont.write((u16) 0); // Functional mode

    std::vector<u16> operations_supported({
        OperationGetDeviceInfo,
        OperationOpenSession,
        OperationCloseSession,
        OperationGetStorageIds,
        OperationGetStorageInfo,
        OperationGetNumObjects,
        OperationGetObjectHandles,
        OperationGetObjectInfo,
        OperationGetObject,
        OperationDeleteObject,
        OperationSendObjectInfo,
        OperationSendObject,
        OperationGetDevicePropDesc,
        OperationGetDevicePropValue,
    });
    cont.write(operations_supported);

    cont.write((u32) 0); // Events supported :(

    std::vector<u16> properties_supported({
        PropertyDeviceFriendlyName,
    });
    cont.write(properties_supported);

    std::vector<u16> formats_supported({
        FormatUndefined,
        FormatAssociation,
    });
    cont.write(formats_supported);

    cont.write(u"Nintendo"); // Manufacturer
    cont.write(u"Nintendo Switch"); // Model
    cont.write(u"1.0"); // Device version
    cont.write(u"SerialNumber"); // Serial number

    printf("BEFORE WRITE CONTAINER\n");
    this->writeContainer(cont);
    printf("AFTER WRITE CONTAINER\n");

    resp->code = ResponseOk;

    printf("END GetDeviceInfo\n"); // This gets printed but it still crashes afterward
}

void MTPResponder::OpenSession(MTPOperation op, MTPResponse *resp) {
    /*if (op.params[0] == 0) {
        resp->code = ResponseInvalidParameter;
    } else */if (this->session_id == 0) {
        this->session_id = op.params[0];
        resp->code = ResponseOk;
    } else {
        resp->code = ResponseSessionAlreadyOpen;
    }
}

void MTPResponder::CloseSession(MTPOperation op, MTPResponse *resp) {
    if (this->session_id == 0) {
        resp->code = ResponseSessionNotOpen;
    } else {
        this->session_id = 0;
        resp->code = ResponseOk;
    }
}

void MTPResponder::GetStorageIds(MTPOperation op, MTPResponse *resp) {
    MTPContainer cont = this->createDataContainer(op);
    cont.write((u32) this->storages.size());
    for (auto store : this->storages) {
        cont.write(store.first);
    }
    this->writeContainer(cont);

    resp->code = ResponseOk;
}

void MTPResponder::GetStorageInfo(MTPOperation op, MTPResponse *resp) {
    MTPContainer cont = this->createDataContainer(op);

    auto info = this->storages[op.params[0]];
    
    if (info.first == "sdmc")
        cont.write((u16) 2); // Storage type
    else 
        cont.write((u16) 1);

    cont.write((u16) 2); // Access Capability

    FsFileSystem *dev = fsdevGetDeviceFileSystem(info.first.c_str());
    
    u64 total, free;
    fsFsGetTotalSpace(dev, "/", &total);
    fsFsGetFreeSpace(dev, "/", &free);
    cont.write(total); // Max capacity
    cont.write(free); // Free Space in bytes

    fsFsClose(dev);

    cont.write((u32) 0xFFFFFFFF); // Free space in objects
    cont.write(info.second); // Storage Description
    cont.write(info.second); // Volume Identifier

    this->writeContainer(cont);

    resp->code = ResponseOk;
}

void MTPResponder::GetObjectHandles(MTPOperation op, MTPResponse *resp) {
    printf("GET OBJECT HANDLES\n");
    auto info = this->storages[op.params[0]];

    MTPContainer cont = this->createDataContainer(op);
    std::vector<u32> handles;

    std::string dir;

    if (op.params[2] == 0xFFFFFFFF) 
        dir = info.first + ":/";
    else
        dir = this->object_handles[op.params[2]];
    printf("DIR: %s\n", dir.c_str());

    for (const auto & entry : fs::directory_iterator(dir)) {
        printf("ITERATE\n");

        printf("PATH\n");
        fs::path path = entry.path();

        printf("GET HANDLE\n");
        u32 handle = this->getObjectHandle(path);
        printf("OBJECT: 0x%x %s\n", handle, path.c_str());

        printf("BEFORE PUSH BACK\n");
        handles.push_back(handle);
        printf("AFTER PUSH BACK\n");

        printf("DONE ITERATE\n");
    }
    printf("DONE WITH FOR\n");

    /*printf("BEFORE OPEN DIR: %s\n", dir.c_str());
    DIR *dir_handle = opendir(dir.c_str()); // This just flat out fails for some reason
    printf("AFTER OPEN DIR\n");
    if (dir_handle == NULL){
        printf("WOOPSIE\n");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir_handle))) {
        printf("ITERATE\n");

        fs::path path = dir + entry->d_name;

        u32 handle = this->getObjectHandle(path);

        printf("OBJECT: 0x%x %s\n", handle, path.c_str());

        printf("BEFORE PUSH BACK\n");
        handles.push_back(handle);
        printf("AFTER PUSH BACK\n");

        printf("DONE ITERATE\n");
    }
    closedir(dir_handle);
    printf("CLOSE DIR\n");*/

    cont.write(handles);
    this->writeContainer(cont);

    resp->code = ResponseOk;
}

void MTPResponder::GetObjectInfo(MTPOperation op, MTPResponse *resp) {
    MTPContainer cont = this->createDataContainer(op);

    fs::path path = this->object_handles[op.params[0]];

    std::string drive = path.root_name().string();
    drive.resize(drive.size() - 1);
    drive.shrink_to_fit();

    u32 storage_id;
    for (auto store : this->storages) {
        if (store.second.first == drive) {
            storage_id = store.first;
            break;
        }
    }

    cont.write(storage_id); // Storage ID
    cont.write((u16) FormatAssociation); // Object Format
    cont.write((u16) 0); // Protection Status

    std::error_code ec;
    u32 size = fs::file_size(path, ec);
    if (ec.value() < 0)
        size = 0;
    cont.write(size); // Object Compressed Size

    cont.write((u16) FormatUndefined); // Thumb Format
    cont.write((u32) 0); // Thumb Compressed Size
    cont.write((u32) 0); // Thumb Pix Width
    cont.write((u32) 0); // Thumb Pix Height
    cont.write((u32) 0); // Image Pix Width
    cont.write((u32) 0); // Image Pix Height
    cont.write((u32) 0); // Image Bit Depth

    fs::path parent = path.parent_path();
    if (parent.string() == this->storages[storage_id].first + ":/")
        cont.write((u32) 0); // Parent Object
    else
        cont.write(this->getObjectHandle(parent.string()));

    cont.write((u16) 1); // Association Type
    cont.write((u32) 1); // Association Description
    cont.write((u32) 0); // Sequence Number
    cont.write(path.filename().u16string()); // Filename

    cont.write(u"20190505T000000"); // Date Created
    cont.write(u"20190505T000000"); // Date Modified

    cont.write(u""); // Keywords

    this->writeContainer(cont);

    resp->code = ResponseOk;
}