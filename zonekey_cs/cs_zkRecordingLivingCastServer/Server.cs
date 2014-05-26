using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Net;

namespace cs_zkRecordingLivingCastServer
{
    class Server
    {
        public Server()
        {
            Log.log("============= Service Start =============");
            server = new MyHttpServer(RequestHandler);
        }

        public void Wait()
        {
            server.Wait();
        }

        public void Close()
        {
            server.Close();
            Log.log("==========================================");
        }

        MyHttpServer server = null;

        void RequestHandler(HttpListenerRequest req, HttpListenerResponse res)
        {
            Console.WriteLine("[RequestHandler: req.url=" + req.Url.ToString());

            if (req.Url.AbsolutePath == "/cmd/record/start") {
                Record.Start(req, res);
            }
            else if (req.Url.AbsolutePath == "/cmd/record/stop") {
                Record.Stop(req, res);
            }
            else if (req.Url.AbsolutePath == "/cmd/livingcast/start") {
                LivingCast.Start(req, res);
            }
            else if (req.Url.AbsolutePath == "/cmd/livingcast/stop") {
                LivingCast.Stop(req, res);
            }
            else {
                res.StatusCode = 404;
                res.ContentType = "text/plain";

                try
                {
                    StreamWriter sw = new StreamWriter(res.OutputStream);
                    sw.WriteLine("NOT supported command: " + req.Url.AbsolutePath);
                    sw.Close();
                }
                catch { }
            }
        }
    }
}
