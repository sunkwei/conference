using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ServiceModel;
using System.Threading;
using ZQNB.Gateway.LiveService;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 对应着一次直播，可能包含多个 LivingcastStream
    /// </summary>
    class LivingcastTask
    {
        const string config_filename_ = @"c:\zonekey.config\RecordingLivingcastServer\nabo.conf";
        KVConfig config = new KVConfig(config_filename_);

        /// <summary>
        /// 启动直播
        /// </summary>
        /// <param name="param"></param>
        public LivingcastTask(LivingcastStartParam param)
        {
            StartParam = param;

            if (!config.ContainsKey("fms_ip"))
                config["fms_ip"] = "127.0.0.1";

            if (!config.ContainsKey("cloud_ip"))
                config["cloud_ip"] = "127.0.0.1";

            if (!config.ContainsKey("cloud_port"))
                config["cloud_port"] = "80";

            if (!config.ContainsKey("teacher"))
                config["teacher"] = "admin";

            if (!config.ContainsKey("grade"))
                config["grade"] = "";

            if (!config.ContainsKey("domain"))
                config["domain"] = "space.whhc";

            if (!config.ContainsKey("device_id"))
                config["device_id"] = "000000000000";

            if (!config.ContainsKey("title"))
                config["title"] = "互动";

            Streams = new List<LivingcastStream>();
            foreach (LivingcastStreamDesc desc in param.streams) {
                string rtmp_url = build_rtmp_livingcast_url(param.mcu_cid, desc);
                Streams.Add(new LivingcastStream(param.mcu_jid, param.mcu_cid, desc.aid, desc.vid, rtmp_url));
            }

            // 发布到nabo平台
            SendStart(param.uuid);

            th_ = new Thread(new ParameterizedThreadStart(proc_HeartBeat));
            th_.Start(new Params() { machine_id = config["device_id"], uuid = param.uuid, });
        }

        Thread th_ = null;

        /// <summary>
        /// 结束直播
        /// </summary>
        public void Close()
        {
            evt_quit_.Set();
            th_.Join();

            // 通知nabo平台，停止转播
            SendStop();

            foreach (LivingcastStream s in Streams) {
                s.Close();
            }

            Streams.Clear();
        }

        class Params
        {
            public string machine_id;
            public string uuid;
        };

        /// <summary>
        /// 心跳线程，需要每隔5秒发送一次
        /// </summary>
        void proc_HeartBeat(object p)
        {
            Params param = (Params)p;

            while (!evt_quit_.WaitOne(5000)) {
                LiveCourseNotifyClient client = new LiveCourseNotifyClient();
                Uri url = client.Endpoint.Address.Uri;
                client.Endpoint.Address = new EndpointAddress(url.Scheme + "://" + config["cloud_ip"] + ":" + config["cloud_port"] + url.PathAndQuery);
                try {
                    client.HeartBeat(param.machine_id);
                }
                catch {
                    client.Abort();
                }
                finally {
                    client.Close();
                }
            }
        }

        AutoResetEvent evt_quit_ = new AutoResetEvent(false);

        void SendStart(string uuid)
        {
            LiveCourseNotifyClient client = new LiveCourseNotifyClient();
            Uri url = client.Endpoint.Address.Uri;
            client.Endpoint.Address = new EndpointAddress(url.Scheme + "://" + config["cloud_ip"] + ":" + config["cloud_port"] + url.PathAndQuery);
            Log.log(string.Format("[LivingcastTask] SendStart: wsurl={0}\n", client.Endpoint.Address.ToString()));
            Live live = new Live()
            {
                Title = config["title"],
                Teacher = config["teacher"],
                Owner = config["teacher"] + "@" + config["domain"],
                Subject = StartParam.subject,
                Grade = config["grade"],
                Guid = Guid.Parse(uuid),
                LiveUrls = new LiveUrl[1],
            };

            foreach (LivingcastStream s in Streams) {
                live.LiveUrls[0] = new LiveUrl() {
                    Url = s.RtmpURL,
                    Tag = s.Mode,
                };
            }

            try {
                ZQNB.Gateway.LiveService.OperationResult result = client.NotifyRecordingStart(live, config["device_id"]);
                Log.log("\ten, success=" + result.Success + ", message=" + result.Message);
            }
            catch (CommunicationException) {
                Log.log("\tCommunicationException\n");
                client.Abort();
            }
            catch (TimeoutException) {
                Log.log("\tTimeoutException\n");
                client.Abort();
            }
            catch (Exception) {
                Log.log("\tException\n");
                client.Abort();
            }
            finally {
                if (client != null) {
                    client.Close();
                }
            }
        }

        void SendStop()
        {
            LiveCourseNotifyClient client = new LiveCourseNotifyClient();
            Uri url = client.Endpoint.Address.Uri;
            client.Endpoint.Address = new EndpointAddress(url.Scheme + "://" + config["cloud_ip"] + ":" + config["cloud_port"] + url.PathAndQuery);
            Log.log(string.Format("[LivingcastTask] SendStop: wsurl={0}\n", client.Endpoint.Address.ToString()));

            try {
                ZQNB.Gateway.LiveService.OperationResult result = client.NotifyRecordingStop(config["device_id"]);
                Log.log("\ten, success=" + result.Success + ", message=" + result.Message + "\n");
            }
            catch (CommunicationException) {
                client.Abort();
                Log.log("\tCommunicationException!\n");
            }
            catch (TimeoutException) {
                client.Abort();
                Log.log("\tTimeoutExeption!\n");
            }
            catch (Exception) {
                client.Abort();
                Log.log("\tException\n");
            }
            finally {
                if (client != null) {
                    client.Close();
                }
            }
        }

        /// <summary>
        /// 生成 rtmp url
        /// </summary>
        /// <param name="cid"></param>
        /// <param name="sd"></param>
        /// <returns></returns>
        string build_rtmp_livingcast_url(int cid, LivingcastStreamDesc sd)
        {
            string url = string.Format("rtmp://{0}/livepkgr/{1}_{2}?adbe-live-event=liveevent", config["fms_ip"], cid, sd.vid);
            return url;
        }

        LivingcastStartParam StartParam { get; set; }
        List<LivingcastStream> Streams { get; set; }
    }
}
