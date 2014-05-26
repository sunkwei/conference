using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using System.Threading;
using System.IO;
using System.Net.Sockets;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// MyHttpServer 启动一个简单的 http server，接收 url 的命令，目前命令包括：
    ///     1. put /cmd/todo
    ///         body:
    ///             <?xml version="1.0" encoding="utf-8" ?>
    ///             <zonekey app="recording_livingcast service">
    ///                 <cmd command="record_start">
    ///                     
    ///                 </cmd>
    ///             </zonekey>
    /// </summary>
    class MyHttpServer
    {
        const string config_filename_ = @"c:\zonekey.config\RecordingLivingcastServer\server.conf";
        KVConfig config_ = new KVConfig(config_filename_);
        string BindingUrl = "http://*:3300/";

        public delegate void DelegateCmdHandler(HttpListenerRequest req, HttpListenerResponse res);
        public MyHttpServer(DelegateCmdHandler handler)
        {
            Handler = handler;
            th = new Thread(new ThreadStart(proc_run));
            th.Start();
            evt_.WaitOne(); // 等待工作线程启动成功
        }

        public void Wait()
        {
            th.Join();
        }

        public void Close()
        {
            if (th != null)
            {
                KillMe();
                th.Join();
            }
        }

        /// <summary>
        /// 工作线程，启动 httpListener ....
        /// </summary>
        void proc_run()
        {
            evt_.Set();

            int port = 3300;
            if (config_.ContainsKey("server_port"))
            {
                BindingUrl = "http://*:" + config_["server_port"] + "/";
                port = int.Parse(config_["server_port"]);
            }

            Log.log("HttpServer starting at port=" + port);

            MseService.Instance(port);

            HttpListener listener = new HttpListener();
            listener.AuthenticationSchemes = AuthenticationSchemes.Anonymous;
            listener.Prefixes.Add(BindingUrl);
            try
            {
                listener.Start();
                Console.WriteLine("启动服务，使用 " + BindingUrl);
                Log.log("        ok");
            }
            catch (Exception e)
            {
                Console.WriteLine("[MyHttpServer] to start " + BindingUrl + " exception: " + e.Message);
                Log.log("HttpServer start ERR: msg=" + e.Message);

                throw new Exception("启动失败，请检查日志");
            }

            Console.WriteLine("[MyHttpServer] starting " + BindingUrl + " OK!");

            bool quit = false;
            do
            {
                HttpListenerContext ctx = listener.GetContext();
                HttpListenerRequest req = ctx.Request;
                HttpListenerResponse res = ctx.Response;

                if (req.Url.AbsolutePath == "/cmd/killme")
                {
                    quit = true;

                    try {
                        res.StatusCode = 200;
                        StreamWriter sw = new StreamWriter(res.OutputStream);
                        sw.Write("OK: server will be killed.");
                        sw.Close();
                        //res.Close();
                    }
                    catch { }
                }
                else
                {
                    Handler(req, res);
                    res.Close();
                }
            } while (!quit);
        }

        /// <summary>
        /// 发送 GET /cmd/killme 
        /// </summary>
        void KillMe()
        {
            int port = 3300;
            if (config_.ContainsKey("server_port"))
                port = int.Parse(config_["server_port"]);

            string str = string.Format("GET http://localhost:{0}/cmd/killme HTTP/1.0\r\n\r\n", port);

            TcpClient tc = new TcpClient("localhost", port);
            StreamWriter sw = new StreamWriter(tc.GetStream());
            sw.Write(str);
            sw.Close();
            tc.Close();
        }

        Thread th { get; set; }
        AutoResetEvent evt_ = new AutoResetEvent(false);
        DelegateCmdHandler Handler { get; set; }
    }
}
