using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace cs_mcu_monitor
{
    public partial class ShowConference_1 : TabPage, ShowStats
    {
        public ShowConference_1()
        {
            InitializeComponent();
        }

        public void UpdateConference(Conference conf, Conference conf_old, double delta)
        {
            // TODO: 显示信息
            Info.Clear();

            Info.Text = string.Format("cid={0}, 主题={1}\r\n", conf.cid, conf.desc);
            Info.Text += string.Format("uptime={0}, 模式={1}\r\n\r\n", conf.uptime, conf.mode);

            Info.Text += "======== STREAMS ============\r\n";
            foreach (Conference.Stream s in conf.Streams) {
                double kbps = 0.0;
                Conference.Stream os = findStream(conf_old, s.streamid);
                if (os != null && delta > 0.9) {
                    kbps = (s.stat.bytes_recv - os.stat.bytes_recv) * 8 / 1000.0 / delta;
                }
                Info.Text += string.Format("{0}:\r\n\trecv={2}, lost={3}, jitter={4}, kbps={1}\r\n",
                    s.desc, string.Format("{0:00.000}", kbps), s.stat.packet_recv, s.stat.packet_lost_recv, s.stat.jitter);
            }

            Info.Text += "======== SINKS ==========\r\n";
            foreach (Conference.Sink s in conf.Sinks) {
                double kbps = 0.0;
                Conference.Sink os = findSink(conf_old, s.sinkid);
                if (os != null && delta > 0.9) {
                    kbps = (s.stat.sent - os.stat.sent) * 8 / 1000.0 / delta;
                }
                Info.Text += string.Format("{0}->{4}:\r\n\tsent={1}, lost={2}, jitter={3}, kpbs={5}\r\n",
                    s.desc, s.stat.packets, s.stat.packets_lost, s.stat.jitter, s.who, string.Format("{0:00.000}", kbps));
            }
        }

        Conference.Stream findStream(Conference conf, int sid)
        {
            foreach (Conference.Stream s in conf.Streams) {
                if (sid == s.streamid)
                    return s;
            }
            return null;
        }

        Conference.Sink findSink(Conference conf, int sid)
        {
            foreach (Conference.Sink s in conf.Sinks) {
                if (sid == s.sinkid)
                    return s;
            }
            return null;
        }
    }
}
