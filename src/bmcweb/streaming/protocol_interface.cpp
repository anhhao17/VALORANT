#include "protocol_interface.hpp"
#include "mjpeg_protocol.hpp"
#include "udp_rtp_protocol.hpp"
#include "rtsp_protocol.hpp"
#include "webrtc_protocol.hpp"
#include "hls_protocol.hpp"
#include "../logging.hpp"

namespace embed::bmcweb::streaming
{

std::unique_ptr<IProtocol> ProtocolFactory::createProtocol(StreamProtocol protocol)
{
    switch (protocol)
    {
        case StreamProtocol::MJPEG:
            LOG_INFO("Creating MJPEG protocol instance");
            return std::make_unique<MjpegProtocol>();
            
        case StreamProtocol::UDP_RTP:
            LOG_INFO("Creating UDP/RTP protocol instance");
            return std::make_unique<UdpRtpProtocol>();
            
        case StreamProtocol::RTSP:
            LOG_INFO("Creating RTSP protocol instance");
            return std::make_unique<RtspProtocol>();
            
        case StreamProtocol::WEBRTC:
            LOG_INFO("Creating WebRTC protocol instance");
            return std::make_unique<WebrtcProtocol>();
            
        case StreamProtocol::HLS:
            LOG_INFO("Creating HLS protocol instance");
            return std::make_unique<HlsProtocol>();
            
        default:
            LOG_ERROR("Unsupported protocol type: {}", static_cast<int>(protocol));
            return nullptr;
    }
}

std::vector<StreamProtocol> ProtocolFactory::getSupportedProtocols()
{
    return {
        StreamProtocol::MJPEG,
        StreamProtocol::UDP_RTP,
        StreamProtocol::RTSP,
        StreamProtocol::WEBRTC,
        StreamProtocol::HLS
    };
}

bool ProtocolFactory::isProtocolSupported(StreamProtocol protocol)
{
    auto supported = getSupportedProtocols();
    for (auto supportedProtocol : supported)
    {
        if (supportedProtocol == protocol)
        {
            return true;
        }
    }
    return false;
}

} // namespace embed::bmcweb::streaming