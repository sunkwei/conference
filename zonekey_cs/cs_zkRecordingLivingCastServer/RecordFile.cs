using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 对应一个录像文件，一般由两个 zkmcu_mod（音频+视频）和一个 zkmcu_record 对象组成
    /// </summary>
    class RecordFile
    {
        public RecordFile(string mcu_jid, string filename, int cid, int audio_sid, int video_sid)
        {
            do_start(mcu_jid, filename, cid, audio_sid, video_sid);
        }

        /// <summary>
        /// 必须调用，以关闭录像文件
        /// </summary>
        public void Close()
        {
            quit_ = true;
            Console.WriteLine("this=" + this);
            Console.Write("a");
            audio_recver_.Stop();
            Console.Write("A");
            Console.Write("v");
            video_recver_.Stop();
            Console.Write("V");

            Console.Write("f");
            rec_flv_.Stop();
            Console.WriteLine("F");
        }

        #region 内部实现
        bool quit_ = false;
        zkmcu_modLib.ZonekeyMCU_MOD audio_recver_ = new zkmcu_modLib.ZonekeyMCU_MOD();
        zkmcu_modLib.ZonekeyMCU_MOD video_recver_ = new zkmcu_modLib.ZonekeyMCU_MOD();
        zkmcu_recordLib.RecordFLV rec_flv_ = new zkmcu_recordLib.RecordFLV();

        #region 启动录像
        int do_start(string mcu_jid, string filename, int cid, int audio_sid, int video_sid)
        {
            audio_recver_.MCUJid = mcu_jid;
            audio_recver_.CBMediaFrameGot += new zkmcu_modLib._IZonekeyMCU_MODEvents_CBMediaFrameGotEventHandler(audio_recver__CBMediaFrameGot);
            audio_recver_.CBError += new zkmcu_modLib._IZonekeyMCU_MODEvents_CBErrorEventHandler(audio_recver__CBError);

            video_recver_.MCUJid = mcu_jid;
            video_recver_.CBMediaFrameGot += new zkmcu_modLib._IZonekeyMCU_MODEvents_CBMediaFrameGotEventHandler(video_recver__CBMediaFrameGot);
            video_recver_.CBError += new zkmcu_modLib._IZonekeyMCU_MODEvents_CBErrorEventHandler(video_recver__CBError);

            rec_flv_.Open(filename, 3); // 3 表示同时包含音视频
            video_recver_.Start(cid, 100, video_sid);
            audio_recver_.Start(cid, 102, audio_sid);

            return 0;
        }

        void video_recver__CBError(int code, string some_info)
        {
            if (!quit_)
                Console.WriteLine("ERR: video stream ...");
        }

        void audio_recver__CBError(int code, string some_info)
        {
            if (!quit_)
                Console.WriteLine("ERR: audio stream ...");
        }

        bool key_video_got = false;
        double video_begin_ = -1.0;
        void video_recver__CBMediaFrameGot(int payload, double stamp, object data, int key)
        {
            if (!quit_)
            {
                if (!key_video_got)
                {
                    if (key != 0) key_video_got = true;
                    video_begin_ = stamp;
                }

                try
                {
                    rec_flv_.SaveH264Frame(stamp - video_begin_, key, data);
                }
                catch { }
            }
        }

        double audio_begin_ = -1.0;
        void audio_recver__CBMediaFrameGot(int payload, double stamp, object data, int key)
        {
            if (!quit_)
            {
                // 需要等待首先写入一帧视频后，再写入音频
                if (key_video_got)
                {
                    if (audio_begin_ < 0.0)
                        audio_begin_ = stamp;

                    try
                    {
                        rec_flv_.SaveAACFrane(stamp - audio_begin_, 1, data);
                    }
                    catch { }
                }
            }
        }
        #endregion

        #endregion
    }
}
