using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 封装一个全局 user，用于向其他 mse service 发送命令
    /// </summary>
    class NormalUser
    {
        public static NormalUser Instance()
        {
            if (instance == null)
                instance = new NormalUser();
            return instance;
        }

        NormalUser()
        {
            User = cs_zkrobot.client.UserFactory.makeNormal();
            if (!User.login("normaluser@" + Utility.Mse.xmpp_domain(), "ddkk1212"))
            {
                throw new Exception("无法登录到 xmpp server");
            }
        }
        static NormalUser instance { set; get; }
        cs_zkrobot.client.NormalUser User { set; get; }
    }
}
