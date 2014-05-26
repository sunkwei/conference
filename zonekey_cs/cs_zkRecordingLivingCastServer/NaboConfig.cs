using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 用于存储到纳博的配置信息
    /// </summary>
    class NaboConfig
    {
        const string cfg_file_ = @"c:\zonekey.config\RecordingLivingcastServer\nabo.conf";

        public static NaboConfig Instance()
        {
            if (instance_ == null)
                instance_ = new NaboConfig();
            return instance_;
        }

        /// <summary>
        /// 设备ID，对应着 meta.xml 文件中的 From
        /// </summary>
        public string DeviceID
        {
            get;
            private set;
        }

        /// <summary>
        /// 年级，对应着 meta.xml 文件文件中的 Grade
        /// </summary>
        public string Grade
        {
            get;
            private set;
        }

        /// <summary>
        /// 对应着 meta.xml 中的 To 的 @ 之后部分
        /// </summary>
        public string Domain
        {
            get;
            private set;
        }

        /// <summary>
        /// 对应着 meta.xml 中的 Teacher, 并 Teacher + '@' + Domain 构成 meta.xml 中的 To
        /// </summary>
        public string Teacher
        {
            get;
            private set;
        }

        /// <summary>
        /// 对应着 meta.xml 中的 Title
        /// </summary>
        public string Title
        {
            get;
            private set;
        }

        /// <summary>
        /// 返回到纳博的ftp服务器
        /// ftp://anonymous:1@192.168.1.10/recording
        /// </summary>
        public string FtpPath
        {
            get;
            private set;
        }

        #region 内部实现
        static NaboConfig instance_ = null;
        KVConfig cfg_ = null;

        NaboConfig()
        {
            //  使用配置文件
            cfg_ = new KVConfig(cfg_file_);

            DeviceID = "000000000000";
            if (cfg_.ContainsKey("device_id"))
                DeviceID = cfg_["device_id"];
            Grade = "";
            if (cfg_.ContainsKey("grade"))
                Grade = cfg_["grade"];
            Domain = "space.whhc";
            if (cfg_.ContainsKey("domain"))
                Domain = cfg_["domain"];
            Teacher = "Admin";
            if (cfg_.ContainsKey("teacher"))
                Teacher = cfg_["teacher"];
            Title = "互动录像";
            if (cfg_.ContainsKey("title"))
                Title = cfg_["title"];
            FtpPath = "ftp://anonymous:1@172.16.1.10/shared";
            if (cfg_.ContainsKey("ftppath"))
                FtpPath = cfg_["ftppath"];
        }

        #endregion
    }
}
