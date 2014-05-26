using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 注册为 mse 服务，类型为 mcu_recording_livingcast_service
    /// </summary>
    class MseService
    {
        const string ServiceType = "mcu_recording_livingcast_service";

        public static MseService Instance(int port)
        {
            if (instance == null)
                instance = new MseService(port);
            return instance;
        }

        MseService(int port)
        {
            Mse = cs_zkrobot.zkRobot_Factory.getRobotMse(Utility.Mse.xmpp_domain());

            // 使用 3300 端口

            Mse.reg_service("mse_s_000000000000_" + ServiceType + "_0", "", ServiceType, "http://" + Utility.Mse.getip() + ":" + port.ToString(), handler, null);
        }

        void handler(cs_zkrobot.zkRobot robot, object userdata, string from_jid, string command, string cmd_options, ref string result, ref string result_options)
        {
            // TODO： 增加命令 ...
            result = "err";
            result_options = "unsupported cmd:" + command;
        }

        static MseService instance { get; set; }
        cs_zkrobot.zkRobot_mse Mse { get; set; }
    }
}
