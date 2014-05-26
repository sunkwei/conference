using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cs_mcu_monitor
{
    /// <summary>
    /// 对应一个 xmpp NormalUser，单件，提供到 mcu 的查询
    /// </summary>
    class UAC
    {
        const string USERNAME = "normaluser";
        const string PASSWD = "ddkk1212";
        const string ENV_DOMAIN = "xmpp_domain";

        #region 公共方法
        public static UAC Instance()
        {
            if (_instance == null)
                _instance = new UAC();
            return _instance;
        }

        /// <summary>
        /// 获取正在进行的互动的 cid 列表
        /// </summary>
        /// <param name="cids"></param>
        public delegate void delegate_list_conference_result(List<int> cids);
        public int ListConferences(delegate_list_conference_result result)
        {
            if (user_ == null) return -1;
            user_.async_request(MCUJid, "list_conferences", null, new cs_zkrobot.Notify_Response(response), result);
            return 0;
        }

        public delegate void delegate_info_conference_result(string str_json);
        public int InfoConference(int cid, delegate_info_conference_result result)
        {
            if (user_ == null) return -1;
            user_.async_request(MCUJid, "info_conference", "cid=" + cid, new cs_zkrobot.Notify_Response(response), result);
            return 0;
        }

        #endregion

        #region 内部实现
        UAC()
        {
            string domain = Environment.GetEnvironmentVariable(ENV_DOMAIN);
            if (domain == null)
                throw new Exception("EXCEPTION: 必须提供环境变量" + ENV_DOMAIN);

            user_ = cs_zkrobot.client.UserFactory.makeNormal();
            if (!user_.login(USERNAME + "@" + domain + "/mcu_monitor", PASSWD))
                throw new Exception("EXCEPTION: normal user 登录 domain=" + domain + " 失败，请管理员解决");

            Domain = domain;

            /// FIXME: 应该通过 root service 查询 "mcu" 服务类型获取！
            MCUJid = "mse_s_000000000000_mcu_0@" + domain;
        }

        string Domain { get; set; }
        string MCUJid { get; set; }

        static UAC _instance = null;
        cs_zkrobot.client.NormalUser user_ = null;

        /// <summary>
        /// 诸如 ”cids=1,2&info=no“，如果 key='cids'，则返回 "1,2"
        /// </summary>
        /// <param name="s"></param>
        /// <param name="key"></param>
        /// <returns></returns>
        string get_kv(string s, string key)
        {
            int pos1 = s.IndexOf(key + '=');
            if (pos1 == -1)
                return null;
            int pos2 = s.IndexOf('&', pos1);
            string sp = null;
            if (pos2 == -1)
                sp = s.Substring(pos1 + key.Length + 1);
            else
                sp = s.Substring(pos1 + key.Length + 1, pos2 - pos1 - key.Length - 1);
            return sp;
        }

        void response(string from_jid, cs_zkrobot.RequestToken token, string result, string options)
        {
            if (token.command() == "list_conferences") {
                if (result == "ok") {
                    // cids=1,2& 
                    List<int> cids = null;

                    string sp = get_kv(options, "cids");
                    if (sp != null) {
                        string[] ss = sp.Split(new char[] { ',' });
                        cids = new List<int>();
                        foreach (string s in ss) {
                            try {
                                int cid = int.Parse(s);
                                cids.Add(cid);
                            }
                            catch { }
                        }
                    }

                    ((delegate_list_conference_result)token.userdata())(cids);
                }
                else {
                    // TODO: 错误处理
                    ((delegate_list_conference_result)token.userdata())(null);
                }
            }
            else if (token.command() == "info_conference") {
                if (result == "ok") {
                    string sp = get_kv(options, "info");
                    ((delegate_info_conference_result)token.userdata())(sp);
                }
                else {
                    // TODO: 错误处理 ...
                    ((delegate_info_conference_result)token.userdata())(null);
                }
            }
            else {
                // TODO: 更多的 ....
            }
        }

        #endregion
    }
}
