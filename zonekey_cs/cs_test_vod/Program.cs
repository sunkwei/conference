using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cs_test_vod
{
    class Program
    {
        static void Main(string[] args)
        {
            // args:
            //      0: jid, name
            //      1: passwd
            //      2: mcu jid
            //      3: cmd
            //      4: cmd_options

            if (args.Length == 5)
            {
                string xmpp_domain = Environment.GetEnvironmentVariable("xmpp_domain");
                if (xmpp_domain == null)
                    xmpp_domain = "app.zonekey.com.cn";

                cs_zkrobot.client.NormalUser user = cs_zkrobot.client.UserFactory.makeNormal();
                if (!user.login(args[0] + "@" + xmpp_domain, args[1]))
                {
                    System.Console.WriteLine("login err: " + args[0] + "@" + xmpp_domain);
                }
                else
                {
                    System.Console.WriteLine("req: to:" + args[2] + ", cmd=" + args[3] + ", options=" + args[4]);
                    System.Threading.AutoResetEvent comp_evt = new System.Threading.AutoResetEvent(false);
                    user.async_request(args[2] + "@" + xmpp_domain, args[3], args[4], cb_response, comp_evt);
                    comp_evt.WaitOne();
                    user.logout();
                }
            }
        }

        static void cb_response(string from_jid, cs_zkrobot.RequestToken token, string result, string options)
        {
            System.Console.WriteLine("res: from:" + from_jid + ", result=" + result + ", options=" + options);
            System.Threading.AutoResetEvent evt = (System.Threading.AutoResetEvent)token.userdata();
            evt.Set();
        }
    }
}
