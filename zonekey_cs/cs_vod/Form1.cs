using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using cs_zkrobot;
using System.Runtime.InteropServices;

namespace cs_vod
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            guiResponse_ = gui_response;
        }

        [DllImport("zk_win_video_render.dll", CallingConvention=CallingConvention.Cdecl)]
        extern static void zkvr_init();
        [DllImport("zk_win_video_render.dll", CallingConvention = CallingConvention.Cdecl)]
        extern static IntPtr zkvr_open(IntPtr hwnd, string ip, int rtp_port, int rtcp_port);
        [DllImport("zk_win_video_render.dll", CallingConvention = CallingConvention.Cdecl)]
        extern static void zkvr_close(IntPtr ins);

        private void Form1_Load(object sender, EventArgs e)
        {
            string xmpp_domain = Environment.GetEnvironmentVariable("xmpp_domain");
            if (xmpp_domain == null)
                xmpp_domain = "app.zonekey.com.cn";

            if (!user_.login("normaluser@" + xmpp_domain, "ddkk1212"))
            {
                MessageBox.Show("登录 '" + xmpp_domain + "' 失败!");
            }
            else
            {
                this.Text = xmpp_domain;

                user_.async_request(get_mcu_jid(xmpp_domain), "test.dc.add_sink", "sid=-1", cb_response, this);
            }
        }

        delegate void GUI_RESPONSE(string from_jid, RequestToken token, string result, string options);
        GUI_RESPONSE guiResponse_ = null;
        IntPtr render_;
        // 
        void cb_response(string from_jid, RequestToken token, string result, string options)
        {
            // 将工作线程转换到窗口线程
            if (this.InvokeRequired)
                this.Invoke(guiResponse_, new object[] { from_jid, token, result, options });
            else
                gui_response(from_jid, token, result, options);
        }

        void gui_response(string from_jid, RequestToken token, string result, string options)
        {
            // 此时在窗口线程中执行了
            if (token.command() == "test.dc.add_sink" && result == "ok")
            {
                SortedDictionary<string, string> kvs = parse_options(options);
                if (kvs.ContainsKey("sinkid") && kvs.ContainsKey("server_ip") && kvs.ContainsKey("server_rtp_port") && kvs.ContainsKey("server_rtcp_port"))
                {
                    int sinkid = int.Parse(kvs["sinkid"]);
                    string ip = kvs["server_ip"];
                    int rtp_port = int.Parse(kvs["server_rtp_port"]);
                    int rtcp_port = int.Parse(kvs["server_rtcp_port"]);

                    // 启动 zk_win_video_render ....
                    zkvr_init();

                    render_ = zkvr_open(this.Handle, ip, rtp_port, rtcp_port);
                }
            }
        }

        SortedDictionary<string, string> parse_options(string str)
        {
            SortedDictionary<string, string> dic = new SortedDictionary<string, string>();
            string[] xx = str.Split(new char[] { '&' });
            foreach (string x in xx)
            {
                string[] kv = x.Split(new char[] { '=' });
                if (kv.Length == 2)
                {
                    dic.Add(kv[0], kv[1]);
                }
            }
            return dic;
        }

        string get_mcu_jid(string domain)
        {
            return "mse_s_000000000000_mcu_0@" + domain;
        }

        cs_zkrobot.client.NormalUser user_ = cs_zkrobot.client.UserFactory.makeNormal();
    }
}
