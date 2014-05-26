using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Newtonsoft.Json.Linq;

namespace cs_mcu_monitor
{
    /// <summary>
    /// 描述一次 info_conference 返回的 json 对象
    /// </summary>
    public class Conference
    {
        public class Stream
        {
            public class Stat
            {
                public Int64 bytes_sent { get; set; }
                public Int64 bytes_recv { get; set; }
                public Int64 packet_sent { get; set; }
                public Int64 packet_recv { get; set; }
                public Int64 packet_lost_sent { get; set; }
                public Int64 packet_lost_recv { get; set; }
                public int jitter { get; set; }
            };

            public int streamid { get; set; }
            public string desc { get; set; }
            public Stat stat { get; set; }
        };

        public class Source
        {
        };

        public class Sink
        {
            public class Stat
            {
                public Int64 sent { get; set; }
                public Int64 packets { get; set; }
                public Int64 packets_lost { get; set; }
                public int jitter { get; set; }
            };

            public int sinkid { get; set; }
            public string desc { get; set; }
            public string who { get; set; }
            public Stat stat { get; set; }
        };

        public Conference(string json_str)
        {
            dynamic json = JObject.Parse(json_str);
            var conference = json["conference"];
            cid = (int)conference["cid"];
            string desc = (string)conference["desc"];
            this.desc = Encoding.GetEncoding("gbk").GetString(Convert.FromBase64String(desc));
            mode = (string)conference["mode"];
            uptime = (double)conference["uptime"];

            Streams = new List<Stream>();
            foreach (var stream in conference["streams"]) {
                Stream s = new Stream();
                s.streamid = stream["streamid"];
                string d = stream["desc"];
                if (d == "audio(iLBC)")
                    s.desc = d;
                else {
                    byte[] raw = Convert.FromBase64String(d);
                    s.desc = Encoding.GetEncoding("gbk").GetString(raw);
                }
                s.stat = new Stream.Stat();
                s.stat.bytes_recv = stream["stat"]["bytes_recv"];
                s.stat.bytes_sent = stream["stat"]["bytes_sent"];
                s.stat.jitter = stream["stat"]["jitter"];
                s.stat.packet_lost_recv = stream["stat"]["packet_lost_recv"];
                s.stat.packet_lost_sent = stream["stat"]["packet_lost_sent"];
                s.stat.packet_recv = stream["stat"]["packet_recv"];
                s.stat.packet_sent = stream["stat"]["packet_sent"];

                Streams.Add(s);
            }

            // TODO: 
            Sources = new List<Source>();

            Sinks = new List<Sink>();
            foreach (var sink in conference["sinks"]) {
                Sink s = new Sink();
                s.sinkid = sink["sinkid"];
                string d = sink["desc"];
                if (d == "audio(iLBC)")
                    s.desc = d;
                else {
                    byte[] raw = Convert.FromBase64String(d);
                    s.desc = Encoding.GetEncoding("gbk").GetString(raw);
                }
                d = sink["who"];
                byte[] who_raw = Convert.FromBase64String(d);
                s.who = Encoding.GetEncoding("gbk").GetString(who_raw);
                s.stat = new Sink.Stat();
                s.stat.jitter = sink["stat"]["jitter"];
                s.stat.packets_lost = sink["stat"]["packets_lost"];
                s.stat.packets = sink["stat"]["packets"];
                s.stat.sent = sink["stat"]["sent"];

                Sinks.Add(s);
            }
        }

        public int cid { get; set; }
        public string desc { get; set; }
        public string mode { get; set; }
        public double uptime { get; private set; }
        public List<Stream> Streams { get; set; }
        public List<Source> Sources { get; set; }
        public List<Sink> Sinks { get; set; }
    };
}
