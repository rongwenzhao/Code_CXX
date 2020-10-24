#include <jni.h>
#include <string>
#include <android/log.h>

#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,"CODE_CXX",__VA_ARGS__)


/*ffmpeg是使用C 语言编写的，C++有重载，而C没有，所以它们俩的方法编译时的存储是不一样的。
 * 所以需要声明该处是C 语言的方式。
 * */

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nick_play_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    hello += avcodec_configuration();
    //初始化解封裝
    av_register_all();
    //初始化網絡
    avformat_network_init();
    //打开文件

    AVFormatContext *ic = NULL;
//    char path[] = "/sdcard/1280x720_1.mp4";
    char path[] = "/sdcard/1280x720_1.flv";
    int re = avformat_open_input(&ic, path, 0, 0);
    if (re != 0) {
        LOGW("avformat_open_input failed!:%s", av_err2str(re));
        return env->NewStringUTF(hello.c_str());
    }

    LOGW("avformat_open_input %s success!", path);
    //獲取流信息
    re = avformat_find_stream_info(ic, 0);
    if (re != 0) {
        LOGW("avformat_find_stream_info failed!");
    }

    LOGW("文件信息 duration = %lld nb_streams = %d", ic->duration, ic->nb_streams);


    //關閉上下文
    avformat_close_input(&ic);
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