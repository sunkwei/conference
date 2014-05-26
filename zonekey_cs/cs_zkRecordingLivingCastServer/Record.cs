using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Net;
using System.Xml;
using System.Diagnostics;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 对应文件描述
    /// </summary>
    class RecordFileDesc
    {
        public int aid { get; set; }
        public int vid { get; set; }
        public string desc { get; set; }
        public string filename { get; set; }

        /// <summary>
        /// 返回是否属性完整
        /// </summary>
        /// <returns></returns>
        public bool Valid()
        {
            return (desc != null) && (filename != null);
        }
    };

    // 启动录像参数
    class RecordStartParam
    {
        public string uuid { get; set; }
        public string subject { get; set; }
        public string mcu_jid { get; set; }
        public int mcu_cid { get; set; }
        public List<RecordFileDesc> files { get; set; }
    };


    /// <summary>
    /// 处理 record/start, 和 record/stop 命令
    /// </summary>
    class Record
    {
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

            Log.log("Record_Start: xml=" + body);

            if (Utility.BodyParser.parse(body, out command, out node, out mcu_jid, out mcu_cid))
            {
                // 需要继续解析 node， 格式为：
                /*
                 *      <cmd command="record_start">
                 *          <uuid>xxxx-xxx...</uuid>        // 录像目录，上传时，将上传整个 uuid 的目录，这个 uuid 将与直播时的 uuid 一致
                 *          <subject>......</subject>       // 互动描述
                 *          <members>
                 *              <caller>...</caller>            // 主讲 ..
                 *              <callee>...</callee>            // 听讲，可能多个
                 *              <callee>...</callee>
                 *          </members>
                 *          <files>
                 *              <file>             // 描述一个录像文件
                 *                  <aid>xx</aid>   // 音频 id
                 *                  <vid>xx</vid>   // 视频 id
                 *                  <desc>....</desc>       // 录像描述
                 *                  <name> ...</name>       // 录像文件名字
                 *                  ....                    // ...
                 *              </file>
                 *              <file>
                 *              </file>
                 *              ....
                 *          </files>
                 *      </cmd>
                 */
                RecordStartParam param = parseRecordStartNode(node);
                if (param == null)
                {
                    format_error(res);
                    return;
                }

                param.mcu_cid = mcu_cid;
                param.mcu_jid = mcu_jid;

                // 检查录像是否已经启动
                lock (RecordingTasks)
                {
                    if (RecordingTasks.ContainsKey(mcu_cid))
                    {
                        already_recording_error(res, mcu_cid);
                        return;
                    }

                    // 启动录像任务
                    try
                    {
                        RecordingTasks.Add(mcu_cid, new RecordTask(param));
                        done(res, "");
                    }
                    catch
                    {
                        failure_error(res);
                    }
                }
            }
            else
            {
                format_error(res);
            }
        }

        public static void Stop(HttpListenerRequest req, HttpListenerResponse res)
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

            Log.log("Record_Stop: xml=" + body);

            if (Utility.BodyParser.parse(body, out command, out node, out mcu_jid, out mcu_cid))
            {
                /** node 格式为：
                 *      <cmd command="record_stop">
                 *      </cmd>
                 */
                lock (RecordingTasks)
                {
                    if (RecordingTasks.ContainsKey(mcu_cid))
                    {
                        RecordingTasks[mcu_cid].Close();
                        RecordingTasks.Remove(mcu_cid);
                        done(res, "");
                    }
                    else
                    {
                        no_matched_recording(res, mcu_cid);
                    }
                }
            }
            else
            {
                format_error(res);
            }
        }


        static RecordFileDesc parseFileDesc(XmlNode node)
        {
            RecordFileDesc desc = new RecordFileDesc();
            foreach (XmlNode n in node.ChildNodes)
            {
                if (n.Name == "aid")
                    desc.aid = int.Parse(n.InnerText);
                else if (n.Name == "vid")
                    desc.vid = int.Parse(n.InnerText);
                else if (n.Name == "desc")
                    desc.desc = n.InnerText;
                else if (n.Name == "name")
                    desc.filename = n.InnerText;
            }

            if (desc.Valid())
                return desc;
            else
                return null;
        }

        static RecordStartParam parseRecordStartNode(XmlElement node)
        {
            RecordStartParam param = new RecordStartParam();
            System.Diagnostics.Debug.Assert(node.Name == "cmd");
//            XmlNode uuid = node.SelectSingleNode("uuid");
//            XmlNode subject = node.OwnerDocument.SelectSingleNode("/zonekey/cmd/subject");
//            XmlNode files = node.OwnerDocument.SelectSingleNode("/zonekey/cmd/files");

            XmlNode uuid = null, subject = null, files = null;
            foreach (XmlNode c in node.ChildNodes)
            {
                if (c.Name == "uuid")
                    uuid = c;
                else if (c.Name == "subject")
                    subject = c;
                else if (c.Name == "files")
                    files = c;
            }

            if (uuid != null && subject != null && files != null)
            {
                param.uuid = uuid.InnerText;
                param.subject = subject.InnerText;
                param.files = new List<RecordFileDesc>();
                foreach (XmlNode file in files.ChildNodes) 
                {
                    RecordFileDesc d = parseFileDesc(file);
                    if (d != null)
                        param.files.Add(d);
                }
                return param;
            }
            else
            {
                return null;
            }
        }

        static void format_error(HttpListenerResponse res)
        {
            res.StatusCode = 551;
            res.StatusDescription = "BadFormat";

            try
            {
                res.ContentType = "text/plain";
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

            try
            {
                res.ContentType = "text/plain";
                StreamWriter sw = new StreamWriter(res.OutputStream);
                sw.WriteLine("DONE: " + info);
                sw.Close();
            }
            catch { }
        }

        static void no_matched_recording(HttpListenerResponse res, int cid)
        {
            res.StatusCode = 553;
            res.StatusDescription = "No Matched Recording";

            try
            {
                res.ContentType = "text/plain";
                StreamWriter sw = new StreamWriter(res.OutputStream);
                sw.WriteLine("cid=" + cid + " NOT recording....");
                sw.Close();
            }
            catch { }
        }

        static void failure_error(HttpListenerResponse res)
        {
            res.StatusCode = 554;
            res.StatusDescription = "Internal Error";

            try
            {
                res.ContentType = "text/plain";
                StreamWriter sw = new StreamWriter(res.OutputStream);
                sw.WriteLine("internal error!!!!!");
                sw.Close();
            }
            catch { }
        }

        /// <summary>
        /// 录像已经启动
        /// </summary>
        /// <param name="res"></param>
        static void already_recording_error(HttpListenerResponse res, int cid)
        {
            res.StatusCode = 552;
            res.StatusDescription = "Already Recording";

            try
            {
                res.ContentType = "text/plain";
                StreamWriter sw = new StreamWriter(res.OutputStream);
                sw.WriteLine("cid=" + cid + " is already recording!");
                sw.Close();
            }
            catch { }
        }

        /// <summary>
        /// 用于保存所有正在进行的录像任务
        /// </summary>
        static SortedDictionary<int, RecordTask> RecordingTasks = new SortedDictionary<int, RecordTask>();
    }
}
