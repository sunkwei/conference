using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 一路到 mcu 的点播，总是指定 一路音频 加 一路视频。
    /// </summary>
    class MediaSessionForMCU
    {
        /// <summary>
        /// 启动一路录像，同时录制视频和音频，写入文件 filename 
        /// </summary>
        /// <param name="filename"></param>
        /// <param name="mcu_jid"></param>
        /// <param name="mcu_cid"></param>
        /// <param name="video_sid"></param>
        /// <param name="audio_sid"></param>
        public MediaSessionForMCU(string filename, string mcu_jid, int mcu_cid, int video_sid, int audio_sid)
        {
            Filename = filename;
            MCUJid = mcu_jid;
            MCUCid = mcu_cid;
            VideoSid = video_sid;
            AudioSid = audio_sid;

            th = new Thread(new ThreadStart(proc_recording));
            th.Start();
        }

        /// <summary>
        /// 必须调用，停止从 mcu 接收数据
        /// </summary>
        public void Close()
        {
            th.Join();
        }

        void proc_recording()
        {
        }

        Thread th { set; get; }
        string Filename { set; get; }
        string MCUJid { set; get; }
        int MCUCid { set; get; }
        int VideoSid { set; get; }
        int AudioSid { set; get; }
    }
}
