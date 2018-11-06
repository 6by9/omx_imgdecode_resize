#ifdef __cplusplus
extern "C"
{
#endif
#include <ilclient.h>
#ifdef __cplusplus
}
#endif
#include <bcm_host.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <dirent.h>

#define MAKE_PIXEL_FORMAT(channels, depth, datatype, order) ((channels) | ((depth) << 4) | ((datatype) << 14) | ((order) << 24))

class LwImage
{
public:
    LwImage() : width(0), height(0), stride(0), format(0) {}
    ~LwImage() {}
    std::vector<uint8_t> pixels;
    int width;
    int height;
    int stride;
    int format;
};

struct OMX_JPEG_DECODER
{
    ILCLIENT_T *ilclient;
    COMPONENT_T *comp_deco;
    COMPONENT_T *comp_resz;
    OMX_HANDLETYPE h_deco;
    OMX_HANDLETYPE h_resz;

    int in_deco;
    int out_deco;

    int in_resz;
    int out_resz;
};

void errorcb(void *userdata, COMPONENT_T *comp, OMX_U32 data)
{
    fprintf(stderr, "ERROR in component %p: %X\n", comp, data);
}

void configchangecb(void *userdata, COMPONENT_T *comp, OMX_U32 data)
{
    fprintf(stderr, "CONF CHANGE in component %p: %d\n", comp, data);
}

void emptybuffercb(void *userdata, COMPONENT_T *comp)
{
    fprintf(stderr, "BUFFER READ in component %p\n", comp);
}

void fillbuffercb(void *userdata, COMPONENT_T *comp)
{
    fprintf(stderr, "BUFFER FILLED in component %p\n", comp);
}

void eoscb(void *userdata, COMPONENT_T *comp, OMX_U32 data)
{
    fprintf(stderr, "EOS in component %p: %d\n", comp, data);
}

void psettingscb(void *userdata, COMPONENT_T *comp, OMX_U32 data)
{
    fprintf(stderr, "PORT SETTINGS CHANGED in component %p: %d\n", comp, data);
}

void init_jpeg_decoder(OMX_JPEG_DECODER *deco)
{
    int omxerr;
    deco->ilclient = ilclient_init();

    ilclient_set_error_callback(deco->ilclient, errorcb, deco);
    ilclient_set_configchanged_callback(deco->ilclient, configchangecb, deco);
    ilclient_set_empty_buffer_done_callback(deco->ilclient, emptybuffercb, deco);
    ilclient_set_eos_callback(deco->ilclient, eoscb, deco);
    ilclient_set_fill_buffer_done_callback(deco->ilclient, fillbuffercb, deco);
    ilclient_set_port_settings_callback(deco->ilclient, psettingscb, deco);

    if ((omxerr = ilclient_create_component(deco->ilclient, &deco->comp_deco, (char*)"image_decode", (ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS|ILCLIENT_ENABLE_INPUT_BUFFERS))) != 0)
    {
        fprintf(stderr, "error creating component %X\n", omxerr);
    }

    deco->h_deco = ILC_GET_HANDLE(deco->comp_deco);

    OMX_PORT_PARAM_TYPE port;

    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    if ((omxerr = OMX_GetParameter(deco->h_deco, OMX_IndexParamImageInit, &port)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_GetParameter(OMX_IndexParamImageInit): %X\n", omxerr);
    }

    deco->in_deco = port.nStartPortNumber;
    deco->out_deco = port.nStartPortNumber + 1;

    if ((omxerr = ilclient_create_component(deco->ilclient, &deco->comp_resz, (char*)"resize", (ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS|ILCLIENT_ENABLE_OUTPUT_BUFFERS))) != 0)
    {
        fprintf(stderr, "error creating component %X\n", omxerr);
    }

    deco->h_resz = ILC_GET_HANDLE(deco->comp_resz);

    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    if ((omxerr = OMX_GetParameter(deco->h_resz, OMX_IndexParamImageInit, &port)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_GetParameter(OMX_IndexParamImageInit): %X\n", omxerr);
    }

    deco->in_resz = port.nStartPortNumber;
    deco->out_resz = port.nStartPortNumber + 1;


    if ((omxerr = ilclient_change_component_state(deco->comp_deco, OMX_StateIdle)) < 0)
    {
        fprintf(stderr, "ilclient_change_component_state(Idle): %d\n", omxerr);
    }

    OMX_IMAGE_PARAM_PORTFORMATTYPE portfmt;


    memset(&portfmt, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    portfmt.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
    portfmt.nVersion.nVersion = OMX_VERSION;
    portfmt.nPortIndex = deco->in_deco;
    portfmt.eCompressionFormat = OMX_IMAGE_CodingJPEG;

    if ((omxerr = OMX_SetParameter(deco->h_deco, OMX_IndexParamImagePortFormat, &portfmt)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SetParameter(OMX_IndexparamImagePortFormat): %X\n", omxerr);
    }

    if ((omxerr = ilclient_enable_port_buffers(deco->comp_deco, deco->in_deco, NULL, NULL, NULL)) < 0)
    {
        fprintf(stderr, "ilclient_enable_port_buffers(deco, in): %d\n", omxerr);
    }

    if ((omxerr = OMX_SetupTunnel(deco->h_deco, deco->out_deco, deco->h_resz, deco->in_resz)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SetupTunnel(deco, out, resize, in): %d\n", omxerr);
    }
    if ((omxerr = OMX_SendCommand(deco->h_deco, OMX_CommandPortEnable, deco->out_deco, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(deco, OMX_CommandPortEnable, out): %d\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_resz, OMX_CommandPortEnable, deco->in_resz, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(resz, OMX_CommandPortEnable, in): %d\n", omxerr);
    }

    if ((omxerr = ilclient_change_component_state(deco->comp_resz, OMX_StateIdle)) < 0)
    {
        fprintf(stderr, "ilclient_change_component_state(resize, Idle): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_deco, OMX_CommandPortEnable, deco->out_deco)) < 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(deco, OMX_CommandPortEnable, out): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_resz, OMX_CommandPortEnable, deco->in_resz)) < 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(resz, OMX_CommandPortEnable, in): %d\n", omxerr);
    }
}

void destroy_jpeg_decoder(OMX_JPEG_DECODER *deco)
{
    COMPONENT_T *components[] = { deco->comp_deco, deco->comp_resz, NULL };
    int omxerr;
    int timeout = 500;

    if ((omxerr = OMX_SendCommand(deco->h_deco, OMX_CommandFlush, deco->in_deco, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(deco, OMX_CommandFlush, in): %X\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_deco, OMX_CommandFlush, deco->out_deco, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(deco, OMX_CommandFlush, out): %X\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_resz, OMX_CommandFlush, deco->in_resz, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(resize, OMX_CommandFlush, in): %X\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_resz, OMX_CommandFlush, deco->out_resz, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(resize, OMX_CommandFlush, out): %X\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_event(deco->comp_deco, OMX_EventCmdComplete, OMX_CommandFlush, 0, deco->in_deco, 0, ILCLIENT_EVENT_ERROR, timeout)) != 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(deco, OMX_CommandFlush, in): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_event(deco->comp_deco, OMX_EventCmdComplete, OMX_CommandFlush, 0, deco->out_deco, 0, ILCLIENT_EVENT_ERROR, timeout)) != 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(deco, OMX_CommandFlush, out): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_event(deco->comp_resz, OMX_EventCmdComplete, OMX_CommandFlush, 0, deco->in_resz, 0, ILCLIENT_EVENT_ERROR, timeout)) != 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(resize, OMX_CommandFlush, in): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_event(deco->comp_resz, OMX_EventCmdComplete, OMX_CommandFlush, 0, deco->out_resz, 0, ILCLIENT_EVENT_ERROR, timeout)) != 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(resize, OMX_CommandFlush, out): %d\n", omxerr);
    }

    ilclient_disable_port_buffers(deco->comp_deco, deco->in_deco, NULL, NULL, NULL);
    ilclient_disable_port_buffers(deco->comp_resz, deco->out_resz, NULL, NULL, NULL);

    if ((omxerr = OMX_SendCommand(deco->h_deco, OMX_CommandPortDisable, deco->out_deco, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(deco, OMX_CommandPortDisable, out): %X\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_resz, OMX_CommandPortDisable, deco->in_resz, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(resize, OMX_CommandPortDisable, in): %X\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_deco, OMX_CommandPortDisable, deco->out_deco)) != 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(deco, OMX_CommandPortDisable, out): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_resz, OMX_CommandPortDisable, deco->in_resz)) != 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(resize, OMX_CommandPortDisable, in): %d\n", omxerr);
    }

    if ((omxerr = OMX_SetupTunnel(deco->h_deco, deco->out_deco, NULL, 0)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SetupTunnel(deco, out, NULL, 0): %X\n", omxerr);
    }

    if ((omxerr = OMX_SetupTunnel(deco->h_resz, deco->in_resz, NULL, 0)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SetupTunnel(resize, in, NULL, 0): %X\n", omxerr);
    }

    ilclient_state_transition(components, OMX_StateLoaded);

    ilclient_cleanup_components(components);
    ilclient_destroy(deco->ilclient);
}

void apply_port_settings(OMX_JPEG_DECODER *deco)
{
    int omxerr;
    int width;
    int height;
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    OMX_STATETYPE last_state;

    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = deco->out_deco;


    if ((omxerr = OMX_SendCommand(deco->h_deco, OMX_CommandPortDisable, deco->out_deco, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(deco, OMX_CommandPortDisable, out): %d\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_resz, OMX_CommandPortDisable, deco->in_resz, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(resz, OMX_CommandPortDisable, in): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_deco, OMX_CommandPortDisable, deco->out_deco)) < 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(deco, OMX_CommandPortDisable, out): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_resz, OMX_CommandPortDisable, deco->in_resz)) < 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(resz, OMX_CommandPortDisable, in): %d\n", omxerr);
    }

    if ((omxerr = OMX_GetParameter(deco->h_deco, OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_GetParameter(deco, OMX_IndexParamPortDefinition, out): %X\n", omxerr);
    }
    else
    {
        width = portdef.format.image.nFrameWidth;
        height = portdef.format.image.nFrameHeight;
    }

    portdef.nPortIndex = deco->in_resz;

    if ((omxerr = OMX_SetParameter(deco->h_resz, OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SetParameter(resz, OMX_IndexParamPortDefinition, in): %X\n", omxerr);
    }

    if ((omxerr = OMX_GetState(deco->h_resz, &last_state)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_GetState(resz): %X\n", omxerr);
    }

    if ((omxerr = ilclient_change_component_state(deco->comp_resz, OMX_StateIdle)) < 0)
    {
        fprintf(stderr, "ilclient_change_component_state(resize, Idle): %d\n", omxerr);
    }

    ilclient_disable_port_buffers(deco->comp_resz, deco->out_resz, NULL, NULL, NULL);

    portdef.nPortIndex = deco->out_resz;

    if ((omxerr = OMX_GetParameter(deco->h_resz, OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_GetParameter(deco, OMX_IndexParamPortDefinition, out): %X\n", omxerr);
    }

    portdef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portdef.format.image.eColorFormat = OMX_COLOR_Format32bitARGB8888;
    portdef.format.image.nFrameWidth = width;
    portdef.format.image.nFrameHeight = height;
    portdef.format.image.nStride = 0;
    portdef.format.image.nSliceHeight = 0;
    portdef.format.image.bFlagErrorConcealment = OMX_FALSE;

    if ((omxerr = OMX_SetParameter(deco->h_resz, OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SetParameter(resz, OMX_IndexParamPortDefinition, in): %X\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_deco, OMX_CommandPortEnable, deco->out_deco, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(deco, OMX_CommandPortEnable, out): %d\n", omxerr);
    }

    if ((omxerr = OMX_SendCommand(deco->h_resz, OMX_CommandPortEnable, deco->in_resz, NULL)) != OMX_ErrorNone)
    {
        fprintf(stderr, "OMX_SendCommand(resz, OMX_CommandPortEnable, in): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_deco, OMX_CommandPortEnable, deco->out_deco)) < 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(deco, OMX_CommandPortEnable, out): %d\n", omxerr);
    }

    if ((omxerr = ilclient_wait_for_command_complete(deco->comp_resz, OMX_CommandPortEnable, deco->in_resz)) < 0)
    {
        fprintf(stderr, "ilclient_wait_for_command_complete(resz, OMX_CommandPortEnable, in): %d\n", omxerr);
    }

    ilclient_enable_port_buffers(deco->comp_resz, deco->out_resz, NULL, NULL, NULL);

    if ((omxerr = ilclient_wait_for_event(deco->comp_resz, OMX_EventPortSettingsChanged, deco->out_resz, 0, 0, 1, ILCLIENT_EVENT_ERROR, 0)) < 0)
    {
        fprintf(stderr, "ilclient_wait_for_event(deco, OMX_eventPortSettingsChanged, out): %d\n", omxerr);
    }

    if (last_state != OMX_StateIdle && (omxerr = ilclient_change_component_state(deco->comp_resz, OMX_StateExecuting)) < 0)
    {
        fprintf(stderr, "ilclient_change_component_state(resize, Exec): %d\n", omxerr);
    }
}

int jpeg_decode(OMX_JPEG_DECODER *deco, const std::vector<uint8_t> &src, LwImage &dst)
{
    COMPONENT_T *components[] = { deco->comp_deco, deco->comp_resz, NULL };
    int omxerr;
    int result = -1;
    const uint8_t *srcOffset = src.data();
    size_t pendingbytes = src.size();
    bool portSettingsChanged = false;

    ilclient_state_transition(components, OMX_StateExecuting);

    while (pendingbytes > 0)
    {
        OMX_BUFFERHEADERTYPE *buffer = ilclient_get_input_buffer(deco->comp_deco, deco->in_deco, 1);

        if (pendingbytes > buffer->nAllocLen)
            buffer->nFilledLen = buffer->nAllocLen;
        else
            buffer->nFilledLen = pendingbytes;

        memcpy(buffer->pBuffer, srcOffset, buffer->nFilledLen);
        srcOffset += buffer->nFilledLen;
        pendingbytes -= buffer->nFilledLen;
        buffer->nOffset = 0;
        buffer->nFlags = (pendingbytes > 0) ? 0 : OMX_BUFFERFLAG_EOS;

        if ((omxerr = OMX_EmptyThisBuffer(deco->h_deco, buffer)) != OMX_ErrorNone)
        {
            fprintf(stderr, "OMX_EmptyThisBuffer(deco, in): %X\n", omxerr);
        }
    }

    if ((omxerr = ilclient_wait_for_event(deco->comp_deco, OMX_EventPortSettingsChanged, deco->out_deco, 0, 0, 1, ILCLIENT_EVENT_ERROR | ILCLIENT_CONFIG_CHANGED | ILCLIENT_PARAMETER_CHANGED | ILCLIENT_BUFFER_FLAG_EOS, 50)) != -2)
    {
        //bool port_settings_triggered = (omxerr == 0);
        if (omxerr == 0)
            apply_port_settings(deco);
        //else
        //    fprintf(stderr, "ilclient_wait_for_event(deco, OMX_eventPortSettingsChanged, out): %d\n", omxerr);

        if ((omxerr = ilclient_wait_for_event(deco->comp_deco, OMX_EventBufferFlag, deco->out_deco, 0, OMX_BUFFERFLAG_EOS, 0, ILCLIENT_EVENT_ERROR | ILCLIENT_CONFIG_CHANGED | ILCLIENT_BUFFER_FLAG_EOS, 50)) == 0)
        {
            OMX_BUFFERHEADERTYPE *buffer = ilclient_get_output_buffer(deco->comp_resz, deco->out_resz, 1);

            if ((omxerr = OMX_FillThisBuffer(deco->h_resz, buffer)) != OMX_ErrorNone)
            {
                fprintf(stderr, "OMX_FillThisBuffer(resize, out): %X\n", omxerr);
            }

            OMX_PARAM_PORTDEFINITIONTYPE portdef;
            portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
            portdef.nVersion.nVersion = OMX_VERSION;
            portdef.nPortIndex = deco->out_resz;

            if (ilclient_remove_event(deco->comp_deco, OMX_EventError, 0, 1, 0, 1) < 0 && (omxerr = ilclient_wait_for_event(deco->comp_resz, OMX_EventBufferFlag, deco->out_resz, 0, OMX_BUFFERFLAG_EOS, 0, ILCLIENT_EVENT_ERROR | ILCLIENT_CONFIG_CHANGED | ILCLIENT_BUFFER_FLAG_EOS, 500)) < 0)
            {
                fprintf(stderr, "ilclient_wait_for_event(resize, OMX_EventBufferFlag, out, EOS): %d\n", omxerr);

                if ((omxerr = ilclient_remove_event(deco->comp_deco, OMX_EventError, 0, 1, 0, 1)) < 0)
                {
                    fprintf(stderr, "ilclient_remove_event(deco, OMX_EventError, *): %d\n", omxerr);
                }
                else
                {
                    fprintf(stderr, "!!! ERROR REMOVED: there wasn't an error? %d\n", omxerr);
                }
            }
            else
            {
                if ((omxerr = OMX_GetParameter(deco->h_resz, OMX_IndexParamPortDefinition, &portdef)) == OMX_ErrorNone)
                {
                    dst.width = portdef.format.image.nFrameWidth;
                    dst.height = portdef.format.image.nFrameHeight;
                    dst.stride = portdef.format.image.nStride;
                    dst.format = MAKE_PIXEL_FORMAT(4, 8, 0, 0);
                    dst.pixels = std::vector<uint8_t>(buffer->pBuffer, buffer->pBuffer + dst.stride * dst.height);
                    result = 0;
                }
                else
                {
                    fprintf(stderr, "OMX_GetParameter(resize, OMX_IndexParamPortDefinition, out): %X\n", omxerr);
                }
            }
        }
        else if (omxerr == -2)
        {
            if ((omxerr = ilclient_remove_event(deco->comp_deco, OMX_EventError, 0, 1, 0, 1)) < 0)
            {
                fprintf(stderr, "ilclient_remove_event(deco, OMX_EventError, *): %d\n", omxerr);
            }
        }

    }
    else
    {
        if ((omxerr = ilclient_remove_event(deco->comp_deco, OMX_EventError, 0, 1, 0, 1)) < 0)
        {
            fprintf(stderr, "ilclient_remove_event(deco, OMX_EventError, *): %d\n", omxerr);
        }
    }

    ilclient_state_transition(components, OMX_StateIdle);

    return result;
}

std::vector<uint8_t> read_file(const std::string &filename)
{
    std::ifstream stream(filename.c_str(), std::ios::in | std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

int main()
{
    OMX_JPEG_DECODER deco;
    int omxerr;

    std::vector<std::pair<std::string,std::vector<uint8_t>>> files;

    bcm_host_init();

    if ((omxerr = OMX_Init()) != OMX_ErrorNone)
        fprintf(stderr, "could'nt initilize OMX: %X", omxerr);

    init_jpeg_decoder(&deco);

    DIR *dp;
    struct dirent *dirp;
    std::string data_path = "./data";

    if ((dp = opendir(data_path.c_str())) != NULL)
    {
        while ((dirp = readdir(dp)) != NULL)
        {
            std::string cf = std::string(dirp->d_name);
            if (cf != "." && cf != "..")
            {
                std::vector<uint8_t> filedata = read_file(data_path + "/" + cf);

                if (filedata.size() > 0)
                {
                    files.push_back(std::pair<std::string, std::vector<uint8_t>>(cf, filedata));
                    std::cout << cf << ": " << filedata.size() << " bytes." << std::endl;
                }
            }

        }
    }

    srand(time(NULL));

    while (true)
    {
        int action = rand() % (files.size() + 1);

        if (action == files.size())
        {
            destroy_jpeg_decoder(&deco);
            init_jpeg_decoder(&deco);
        }
        else
        {
            std::vector<uint8_t> &v = files[action].second;
            LwImage img;

            if (jpeg_decode(&deco, v, img) == 0)
                std::cout << "image decoded: " << files[action].first << "(" << img.width << "x" << img.height << ")" << std::endl;
            else
                std::cout << "!!!error decoding: " << files[action].first << std::endl;
        }
    }

    return 0;
}