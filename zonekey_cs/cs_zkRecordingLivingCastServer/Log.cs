using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace cs_zkRecordingLivingCastServer
{
    class Log
    {
        /// <summary>
        /// 将信息写入 c:/zonekey.config/log/zkrecordinglivingcastService/yyyy-MM-dd.log
        /// 每次写一行
        /// </summary>
        /// <param name="info"></param>
        public static void log(string info)
        {
            const string path = @"c:\\zonekey.config\\log\\RecordingLivingcastService";
            if (!Directory.Exists(path)) {
                try {
                    Directory.CreateDirectory(path);
                }
                catch {}
            }

            if (Directory.Exists(path)) {
                string filename = string.Format("{0}\\{1}.log", path, DateTime.Now.ToString("yyyy-MM-dd"));
                try {
                    StreamWriter sw = new StreamWriter(filename, true);
                    sw.WriteLine(string.Format("{0} {1}", DateTime.Now.ToString("HH:mm:ss"), info));
                    sw.Close();
                }
                catch { }
            }
        }
    }
}
