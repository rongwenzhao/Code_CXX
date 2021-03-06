#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,"CODE_CXX",__VA_ARGS__)


/*ffmpeg是使用C 语言编写的，C++有重载，而C没有，所以它们俩的方法编译时的存储是不一样的。
 * 所以需要声明该处是C 语言的方式。
 * */

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/jni.h>
#include <libswscale/swscale.h>//图像像素格式转换的头文件
#include <libswresample/swresample.h>//音频重采样

}

/**
 * 將AVRational這樣的分數轉爲帶小數點的浮點數。
 * @param r
 * @return
 */
static double r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? 0 : (double) r.num / (double) r.den;
}

//当前时间戳 clock
long long GetNowMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int sec = tv.tv_sec % 360000;//只计算100个小时内的时间计算
    long long t = sec * 1000 + tv.tv_usec / 1000;
    return t;
}

/*extern "C"
JNIEXPORT
jint JNI_OnLoad(JavaVM *vm, void *res) {
    av_jni_set_java_vm(vm, 0);
    return JNI_VERSION_1_4;
}*/

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nick_play_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    hello += avcodec_configuration();

    //关闭上下文
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_nick_play_MainActivity_open(JNIEnv *env, jobject instance, jstring url_, jobject handle) {
    const char *url = env->GetStringUTFChars(url_, 0);
    bool flag = false;
    FILE *fp = fopen(url, "rb");
    if (!fp) {
        LOGW("%s 打开失敗", url);

    } else {
        flag = true;
        LOGW("%s 打开成功 ", url);
        fclose(fp);
    }

    env->ReleaseStringUTFChars(url_, url);
    return flag;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_nick_play_XPlay_Open(JNIEnv *env, jobject instance, jstring url_, jobject surface) {
    const char *path = env->GetStringUTFChars(url_, 0);
    //初始化解封裝
    av_register_all();
    //初始化網絡
    avformat_network_init();

    //注册解码器
    avcodec_register_all();

    //打开文件
    AVFormatContext *ic = NULL;
    //char path[] = "/sdcard/1280x720_1.mp4";
//    char path[] = "/sdcard/1280x720_1.flv";
    int re = avformat_open_input(&ic, path, 0, 0);
    if (re != 0) {
        LOGW("avformat_open_input failed!:%s", av_err2str(re));
        return;
    }

    LOGW("avformat_open_input %s success!", path);
    //獲取流信息
    re = avformat_find_stream_info(ic, 0);
    if (re != 0) {
        LOGW("avformat_find_stream_info failed!");
    }

    LOGW("文件信息 duration = %lld nb_streams = %d", ic->duration, ic->nb_streams);

    //打印音視頻相關的信息
    int fps = 0;
    int videoStream = 0;
    int audioStream = 1;

    for (int i = 0; i < ic->nb_streams; i++) {
        AVStream *as = ic->streams[i];
        if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            LOGW("视频数据");
            videoStream = i;
            //将帧率转换为浮点数
            fps = r2d(as->avg_frame_rate);

            LOGW("fps = %d, width=%d height=%d codeid=%d format=%d", fps,
                 as->codecpar->width,
                 as->codecpar->height,
                 as->codecpar->codec_id,
                 as->codecpar->format
            );
            //codec_id=28 AV_CODEC_ID_H264
            //format=0 AVPixelFormat AV_PIX_FMT_YUV420P


        } else if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            LOGW("音频数据");
            audioStream = i;
            LOGW("sample_rate=%d channels=%d sample_format=%d",
                 as->codecpar->sample_rate,
                 as->codecpar->channels,
                 as->codecpar->format
            );
            //AVSampleFormat sample_format=8 AV_SAMPLE_FMT_FLTP

        }
    }

    //通过使用av_find_best_stream()的方式获取音视频流
    audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    LOGW("av_find_best_stream audioStream = %d", audioStream);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    //打开视频解码器
    //视频 软解码器
    AVCodec *vcodec = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id);

    //硬解码器
    //vcodec = avcodec_find_decoder_by_name("h264_mediacodec");

    if (!vcodec) {
        LOGW("avcodec_find video failed!");
        return;
    }

    //视频 解码器初始化
    AVCodecContext *vc = avcodec_alloc_context3(vcodec);//解码上下文
    avcodec_parameters_to_context(vc, ic->streams[videoStream]->codecpar);//将视频参数赋值到解码器context当中。
    vc->thread_count = 4;//线程数

    //打开视频解码器
    re = avcodec_open2(vc, 0, 0);
    if (re != 0) {
        LOGW("avcodec_open2 video failed!");
        return;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    //打开音频解码器
    //音频 软解码器
    AVCodec *acodec = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id);

    if (!acodec) {
        LOGW("avcodec_find audio failed!");
        return;
    }

    //音频 解码器初始化
    AVCodecContext *ac = avcodec_alloc_context3(acodec);//解码上下文
    avcodec_parameters_to_context(ac, ic->streams[audioStream]->codecpar);//将视频参数赋值到解码器context当中。
    ac->thread_count = 4;//线程数

    //打开音频解码器
    re = avcodec_open2(ac, 0, 0);
    if (re != 0) {
        LOGW("avcodec_open2 audio failed!");
        return;
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //通过 avcodec_send_packet 发送 AVPacket到线程中去解码
    //通过read Frame 读取解码后的数据的过程。



    //读取帧数据
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    long long start = GetNowMs();
    int frameCount = 0;

    //初始化像素格式转换的上下文
    SwsContext *vctx = NULL;
    int outWidth = 1920;
    int outHeight = 1080;
    char *rgb = new char[1920 * 1080 * 4];
    char *pcm = new char[48000 * 4 * 2];//1秒种的音频数据存储

    //音频重采样上下文初始化
    //重采样上下文初始化
    SwrContext *actx = swr_alloc();
    //设置参数
    actx = swr_alloc_set_opts(actx,
                              av_get_default_channel_layout(2),//输出两声道的channel_layout
                              AV_SAMPLE_FMT_S16,
                              ac->sample_rate,//样本率假定不变
                              av_get_default_channel_layout(ac->channels),
                              ac->sample_fmt,
                              ac->sample_rate,
                              0, 0);
    re = swr_init(actx);
    if (re != 0) {
        LOGW("swr_init failed!");
    } else {
        LOGW("swr_init success!");
    }

    //显示窗口的初始化
    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);
    //设置窗口的格式
    ANativeWindow_setBuffersGeometry(nwin, outWidth, outHeight, WINDOW_FORMAT_RGBA_8888);
    //设置后备缓冲
    ANativeWindow_Buffer wbuf;


    for (;;) {

        //超过三秒
        if (GetNowMs() - start >= 3000) {
            LOGW("now decode fps = %d", frameCount / 3);
            start = GetNowMs();
            frameCount = 0;
        }

        int re = av_read_frame(ic, pkt);
        if (re != 0) {
            LOGW("读取到结尾处！");
            int pos = 3 * r2d(ic->streams[videoStream]->time_base);
            //AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME 时间往后找，同时要是关键帧
            av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
            continue;
        }
        LOGW("stream =%d size = %d pts = %lld flag = %d",
             pkt->stream_index, pkt->size, pkt->pts, pkt->flags);
        ////////////////////////////////////// 音，视频帧的发送与解码后帧的读取

        AVCodecContext *cc = vc;
        if (pkt->stream_index == audioStream) {
            cc = ac;
        }


        //发送到线程中解码
        re = avcodec_send_packet(cc, pkt);

        //上面avcodec_send_packet中pkt会被复制一份，所以此处可以直接清理
        av_packet_unref(pkt);

        if (re != 0) {
            LOGW("avcodec_send_packet failed");
            continue;
        }


        //接收解码后的数据帧
        for (;;) {//一直读取解码后的缓冲中的数据，直到读取失败。因为，可能缓冲中是之前的数据，所以要一直读到最后。
            re = avcodec_receive_frame(cc, frame);
            if (re != 0) {
                //LOGW("avcodec_receive_frame failed");
                break;
            }
            //接收成功
            LOGW("avcodec_receive_frame %lld", frame->pts);
            //视频帧
            if (cc == vc) {//视频帧的处理
                frameCount++;
                //初始化像素转换上下文
                vctx = sws_getCachedContext(vctx,
                                            frame->width,
                                            frame->height,
                                            (AVPixelFormat) frame->format,
                                            outWidth,
                                            outHeight,
                                            AV_PIX_FMT_RGBA,
                                            SWS_FAST_BILINEAR,//线性插值的算法
                                            0, 0, 0
                );
                if (!vctx) {
                    LOGW("sws_getCachedContext failed!");
                } else {
                    //转换
                    uint8_t *data[AV_NUM_DATA_POINTERS] = {0};//uint8_t 即 unsigned char
                    data[0] = (uint8_t *) rgb;
                    int lines[AV_NUM_DATA_POINTERS] = {0};//一行宽度的大小
                    lines[0] = outWidth * 4;//输出 AV_PIX_FMT_RGBA格式，乘以4

                    int h = sws_scale(vctx,
                                      frame->data,
                                      frame->linesize, 0,
                                      frame->height,
                                      data, lines);
                    LOGW("sws_scale = %d!", h);
                    if (h > 0) {
                        ANativeWindow_lock(nwin, &wbuf, 0);
                        uint8_t *dst = (uint8_t *) wbuf.bits;
                        memcpy(dst, rgb, outWidth * outHeight * 4);
                        ANativeWindow_unlockAndPost(nwin);
                    }

                }

            } else {//音频的处理
                uint8_t *out[2] = {0};
                out[0] = (uint8_t *) pcm;
                //音频重采样
                int len = swr_convert(actx, out,
                                      frame->nb_samples,
                                      (const uint8_t **) frame->data,
                                      frame->nb_samples);

                LOGW("swr_convert = %d! nb_samples = %d", len, frame->nb_samples);//1024
            }
        }


    }
    delete rgb;
    delete pcm;
    //关闭上下文
    avformat_close_input(&ic);

    env->ReleaseStringUTFChars(url_, path);

}