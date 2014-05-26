using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 直播任务，对应使用录像相同的 uuid，将作为
    /// </summary>
    class LivingcastStream
    {
        public LivingcastStream(string mcu_jid, int mcu_cid, int audio_sid, int video_sid, string rtmp_url)
        {
            RtmpURL = rtmp_url;

            Console.WriteLine("LivingcastStream: 启动直播，cid=" + mcu_cid + ", rtmp_url=" + rtmp_url);

            audio_recver_.MCUJid = mcu_jid;
            audio_recver_.CBMediaFrameGot += new zkmcu_modLib._IZonekeyMCU_MODEvents_CBMediaFrameGotEventHandler(audio_recver__CBMediaFrameGot);

            video_recver_.MCUJid = mcu_jid;
            video_recver_.CBMediaFrameGot += new zkmcu_modLib._IZonekeyMCU_MODEvents_CBMediaFrameGotEventHandler(video_recver__CBMediaFrameGot);

            try {
                rtmp_cast_.Open(rtmp_url);
                CanSend = true;
                Console.WriteLine("OK: create RTMP session, url=" + rtmp_url);

                Log.log("  rtmp url=" + rtmp_url);
            }
            catch {
                Console.WriteLine(" ERR: rtmp_Open err");
                CanSend = false;
                Log.log("  rtmp Open ERR, 需要检查 nabo.conf 的配置");
            }

            audio_recver_.Start(mcu_cid, 102, audio_sid);
            video_recver_.Start(mcu_cid, 100, video_sid);

            if (video_sid == -1)
                Mode = "film";
            else
                Mode = "resources";
        }

        public void Close()
        {
            quit_ = true;
            audio_recver_.Stop();
            video_recver_.Stop();
            rtmp_cast_.Close();
        }

        public string RtmpURL { get; private set; }
        public string Mode { get; private set; }

        #region 内部实现
        bool quit_ = false;
        bool CanSend { get; set; }

        zkmcu_modLib.ZonekeyMCU_MOD audio_recver_ = new zkmcu_modLib.ZonekeyMCU_MOD();
        zkmcu_modLib.ZonekeyMCU_MOD video_recver_ = new zkmcu_modLib.ZonekeyMCU_MOD();
        zkmcu_modLib.ZonekeyMCULivingcast rtmp_cast_ = new zkmcu_modLib.ZonekeyMCULivingcast();

        void video_recver__CBMediaFrameGot(int payload, double stamp, object data, int key)
        {
            if (!quit_ && CanSend) {
                rtmp_cast_.SendH264(stamp, data, key);
            }
        }

        void audio_recver__CBMediaFrameGot(int payload, double stamp, object data, int key)
        {
            if (!quit_ && CanSend) {
                rtmp_cast_.SendAAC(stamp, data);
            }
        }

        #endregion 
    }
}
