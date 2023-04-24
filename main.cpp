#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include </usr/include/jsoncpp/json/json.h>
extern "C" {
//#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
}
void cameraBroadcast(int camNum, std::string cameraAddress, std::string serverAddress) {
    char *camAddr = const_cast<char*>(cameraAddress.c_str());
    char *dvorAddr = const_cast<char*>(serverAddress.c_str());
    char dvorFormat[] = "rtsp";
    long reconnInterval = 30000000000;

    while (1) {
        AVFormatContext *pCamFormatContext = avformat_alloc_context();
        if (!pCamFormatContext) {
            std::cout << "Camera " << camNum << ": Error with allocate format context" << std::endl;
            std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
            continue;
        }
        std::cout << "Camera " << camNum << ": Connecting..." << std::endl;
        if (avformat_open_input(&pCamFormatContext, camAddr, nullptr, nullptr) != 0) {
            std::cout << "Camera " << camNum << ": Could not open the file" << std::endl;
            avformat_free_context(pCamFormatContext);
            std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
            continue;
        }
        if (avformat_find_stream_info(pCamFormatContext, nullptr) < 0) {
            std::cout << "Camera " << camNum << ": Could not get the stream info" << std::endl;
            avformat_free_context(pCamFormatContext);
            std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
            continue;
        }

        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        for (int i = 0; i < pCamFormatContext->nb_streams; i++) {
            AVCodecParameters *pCamLocalCodecParameters = pCamFormatContext->streams[i]->codecpar;
            if (pCamLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) videoStreamIndex = i;
            else if (pCamLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) audioStreamIndex = i;
        }

        AVFormatContext *pDvorFormatContext;
        if (avformat_alloc_output_context2(&pDvorFormatContext, nullptr, dvorFormat, dvorAddr)) {
            std::cout << "Camera " << camNum << ": Could not create output context" << std::endl;
            avformat_free_context(pCamFormatContext);
            std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
            continue;
        }

        AVStream *pDvorVideoStream;
        AVStream *pDvorAudioStream;
        if (videoStreamIndex >= 0) {
            pDvorVideoStream = avformat_new_stream(pDvorFormatContext, NULL);
            if (!pDvorVideoStream) {
                std::cout << "Camera " << camNum << ": Failed allocating output stream" << std::endl;
                avformat_free_context(pCamFormatContext);
                avformat_free_context(pDvorFormatContext);
                std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
                continue;
            }
            if (avcodec_parameters_copy(pDvorVideoStream->codecpar,
                                        pCamFormatContext->streams[videoStreamIndex]->codecpar) < 0) {
                std::cout << "Camera " << camNum << ": Failed to copy codec parameters" << std::endl;
                avformat_free_context(pCamFormatContext);
                avformat_free_context(pDvorFormatContext);
                std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
                continue;
            }
        }
        if (audioStreamIndex >= 0) {
            pDvorAudioStream = avformat_new_stream(pDvorFormatContext, NULL);
            if (!pDvorAudioStream) {
                std::cout << "Camera " << camNum << ": Failed allocating output stream" << std::endl;
                avformat_free_context(pCamFormatContext);
                avformat_free_context(pDvorFormatContext);
                std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
                continue;
            }
            if (avcodec_parameters_copy(pDvorAudioStream->codecpar,
                                        pCamFormatContext->streams[audioStreamIndex]->codecpar) < 0) {
                std::cout << "Camera " << camNum << ": Failed to copy codec parameters" << std::endl;
                avformat_free_context(pCamFormatContext);
                avformat_free_context(pDvorFormatContext);
                std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
                continue;
            }
        }

        //av_dump_format(pCamFormatContext, 0, camAddr, 0);
        //av_dump_format(pDvorFormatContext, 0, dvorAddr, 1);
        AVDictionary *d = NULL;
        av_dict_set(&d, "rtsp_transport", "tcp", 0);
        if (avformat_write_header(pDvorFormatContext, &d) < 0) {
            std::cout << "Camera " << camNum << ": Failed to write header in " << dvorAddr << std::endl;
            avformat_free_context(pCamFormatContext);
            avformat_free_context(pDvorFormatContext);
            av_dict_free(&d);
            std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
            continue;
        }
        av_dict_free(&d);

        AVPacket packet;
        AVStream *pInStream;
        AVStream *pOutStream;
        int64_t lastAudioDts = 0, lastVideoDts = 0;
        int64_t startTs = std::chrono::system_clock::now().time_since_epoch().count();
        std::cout << "Camera " << camNum << ": Broadcasting started." << std::endl;
        for (int x = 0; x < 30;) {
            if (av_read_frame(pCamFormatContext, &packet) < 0) {
                std::cout << "Camera " << camNum << ": disconnected." << std::endl;
                break;
            }
            pInStream = pCamFormatContext->streams[packet.stream_index];
            if (packet.stream_index == videoStreamIndex) {
                pOutStream = pDvorVideoStream;
                if (packet.dts <= lastVideoDts) {
                    packet.dts = ++lastVideoDts;
                    packet.pts = lastVideoDts;
                }
                lastVideoDts = packet.dts;
            } else if (packet.stream_index == audioStreamIndex) {
                pOutStream = pDvorAudioStream;
                if (packet.dts <= lastAudioDts) {
                    packet.dts = ++lastAudioDts;
                    packet.pts = lastAudioDts;
                }
                lastAudioDts = packet.dts;
            } else {
                av_packet_unref(&packet);
                continue;
            }
            packet.pts += startTs;
            packet.dts = packet.pts;
            if (av_interleaved_write_frame(pDvorFormatContext, &packet) < 0) {
                std::cout << "Camera " << camNum << ": Eггог muxing packet." << std::endl;
                break;
            }
            av_packet_unref(&packet);
        }
        av_write_trailer(pDvorFormatContext);
        avformat_close_input(&pCamFormatContext);
        if (pDvorFormatContext && !(pDvorFormatContext->oformat->flags && AVFMT_NOFILE))
            avio_closep(&pDvorFormatContext->pb);
        avformat_free_context(pCamFormatContext);
        avformat_free_context(pDvorFormatContext);
        std::this_thread::sleep_for(std::chrono::nanoseconds(reconnInterval));
    }
}

int main() {
    std::ifstream camcastJson("camcast.json");
    if (!camcastJson) { std::cout << "Can't open camcast.json" << std::endl; return -1; }
    Json::Reader reader;
    Json::Value root;
    reader.parse(camcastJson, root);
    int camsCount = root["cams"].size();
    std::vector <std::thread> th_vec;

    for (int x = 0; x < camsCount; x++) {
        std::string camAddr = root["cams"][x]["cameraAddr"].asString();
        std::string serverAddr = root["cams"][x]["server"].asString();
        th_vec.push_back(std::thread(cameraBroadcast, x+1, camAddr, serverAddr));
        std::this_thread::sleep_for(std::chrono::nanoseconds(300000000));
    }
    for (int i = 0; i < camsCount; i++) {
        th_vec.at (i).join ();
    }

    camcastJson.close();
}
