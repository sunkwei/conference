using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Net;
using System.Xml;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 对应一路直播流
    /// </summary>
    class LivingcastStreamDesc
    {
        public int aid;
        public int vid;
    };

    /// <summary>
    /// 启动直播的参数
    /// </summary>
    class LivingcastStartParam
    {
        public string mcu_jid;
        public int mcu_cid;
        public string uuid;
        public string subject;
        public List<LivingcastStreamDesc> streams = new List<LivingcastStreamDesc>();
    };

    /// <summary>
    /// 处理 /cmd/livingcast/start 和 /cmd/livingcast/stop 命令
    /// </summary>
    class LivingCast
    {
        /// <summary>
        /// body 为 xml 格式
        ///  <zonekey app="record_livingcast_service">
        ///     <cmd command="livingcast_start">
        ///         <uuid>xxxxx</uuid>   与录像一致
        ///         <subject>xxxx</subject> 与录像一致
        ///         <streams>
        ///             <stream>
        ///                 <aid>xx</aid>   音频
        ///                 <vid>xxx</vid> 视频
        ///             </stream>
        ///         </streams>
        ///     </cmd>
        ///     <mcu>
        ///         <jid>xxx</jid>
        ///         <cid>xxx</cid>
        ///     </mcu>
        ///  </zonekey>
        ///  
        /// </summary>
        /// <param name="req"></param>
        /// <param name="res"></param>
        public static void Start(HttpListenerRequest req, HttpListenerResponse res)
        {
            string command, mcu_jid;
            int mcu_cid;
            XmlElement node;

            string body_b64 = Utility.BodyFromStream.read(req.InputStream, req.ContentLength64, Encoding.ASCII);
            string body = Encoding.UTF8.GetString(Convert.FromBase64String(body_b64));
            if (body == null)
            {
                Console.WriteLine("Record.Start: Net broken!");
                return;
            }

            Log.log("Livingcast_Start: xml=" + body);

            if (Utility.BodyParser.parse(body, out command, out node, out mcu_jid, out mcu_cid)) {
                if (command != "livingcast_start") {
                    format_error(res);
                    return;
                }
                LivingcastStartParam start_param = parse_cmd_node(node, mcu_jid, mcu_cid);
                if (start_param == null) {
                    format_error(res);
                    return;
                }

                // 这里启动 ....
                lock (LivingcastTasks) {
                    if (LivingcastTasks.ContainsKey(mcu_cid)) {
                        livingcast_exist(res);
                        return;
                    }
                }

                LivingcastTask task = new LivingcastTask(start_param);
                lock (LivingcastTasks) {
                    LivingcastTasks.Add(mcu_cid, task);
                }

                done(res, "ok");
            }
        }

        public static void Stop(HttpListenerRequest req, HttpListenerResponse res)
        {
            string command, mcu_jid;
            int mcu_cid;
            XmlElement node;

            string body_b64 = Utility.BodyFromStream.read(req.InputStream, req.ContentLength64, Encoding.ASCII);
            string body = Encoding.UTF8.GetString(Convert.FromBase64String(body_b64));
            if (body == null) {
                Console.WriteLine("Record.Start: Net broken!");
                return;
            }

            Log.log("Livingcast_Stop: xml=" + body);

            if (Utility.BodyParser.parse(body, out command, out node, out mcu_jid, out mcu_cid)) {
                if (command != "livingcast_stop") {
                    format_error(res);
                    return;
                }

                // 结束对应的直播任务
                lock (LivingcastTasks) {
                    if (LivingcastTasks.ContainsKey(mcu_cid)) {
                        LivingcastTask task = LivingcastTasks[mcu_cid];
                        task.Close();
                        LivingcastTasks.Remove(mcu_cid);
                    }
                }

                done(res, "ok");
            }            
        }

        static LivingcastStartParam parse_cmd_node(XmlElement node, string mcu_jid, int mcu_cid)
        {
            // 根据 cmd node 构造 LivingcastStartParam

            XmlNode uuid = null, subject = null, streams = null;
            foreach (XmlNode c in node.ChildNodes) {
                if (c.Name == "uuid")
                    uuid = c;
                else if (c.Name == "subject")
                    subject = c;
                else if (c.Name == "streams")
                    streams = c;
            }

            if (uuid == null || subject == null || streams == null) return null;

            LivingcastStartParam param = new LivingcastStartParam();
            param.uuid = uuid.InnerText;
            param.subject = subject.InnerText;
            foreach (XmlNode s in streams.ChildNodes) {
                if (s.Name != "stream") continue;
                LivingcastStreamDesc d = parse_stream_node(s);
                if (d != null)
                    param.streams.Add(d);
            }

            param.mcu_cid = mcu_cid;
            param.mcu_jid = mcu_jid;

            return param;
        }

        static LivingcastStreamDesc parse_stream_node(XmlNode s)
        {
            int aid = -2, vid = -2; // 在 MCU 中，不可能出现 -2 的情况
            foreach (XmlNode n in s.ChildNodes) {
                if (n.Name == "aid")
                    aid = int.Parse(n.InnerText);
                else if (n.Name == "vid")
                    vid = int.Parse(n.InnerText);
            }

            if (aid != -2 && vid != -2) {
                LivingcastStreamDesc desc = new LivingcastStreamDesc();
                desc.aid = aid;
                desc.vid = vid;
                return desc;
            }
            else
                return null;
        }

        static void format_error(HttpListenerResponse res)
        {
            res.StatusCode = 551;
            res.StatusDescription = "BadFormat";

            res.ContentType = "text/plain";
            try
            {
                StreamWriter sw = new StreamWriter(res.OutputStream);
                sw.WriteLine("request format error!");
                sw.Close();
            }
            catch { }
        }

        static void done(HttpListenerResponse res, string info)
        {
            res.StatusCode = 200;
            res.StatusDescription = "OK";

            res.ContentType = "text/plain";
            try
            {
                StreamWriter sw = new StreamWriter(res.OutputStream);
                sw.WriteLine("DONE: " + info);
                sw.Close();
            }
            catch (Exception e)
            {
                string reason = e.Message;
            }
        }

        static void livingcast_exist(HttpListenerResponse res)
        {
            res.StatusCode = 552;
            res.StatusDescription = "LivingcastExist";
        }

        /// <summary>
        /// 使用 mcu_cid 作为 key
        /// </summary>
        static SortedDictionary<int, LivingcastTask> LivingcastTasks = new SortedDictionary<int, LivingcastTask>();
    }
}
