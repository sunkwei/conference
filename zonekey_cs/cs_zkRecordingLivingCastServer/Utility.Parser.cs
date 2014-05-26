using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.IO;
using System.Management;
using System.Net.NetworkInformation;

namespace cs_zkRecordingLivingCastServer
{
	namespace Utility
	{
        /// <summary>
        /// 从 Stream 中读取指定字节数，并构造 string
        /// 比较保守，每次读一个字节 :)
        /// </summary>
        class BodyFromStream
        {
            public static string read(Stream s, long count, Encoding enc)
            {
                string str = "";
                while (count > 0)
                {
                    byte[] buffer = new byte[1];
                    try
                    {
                        s.Read(buffer, 0, 1);
                    }
                    catch
                    {
                        return null;
                    }

                    str += enc.GetString(buffer);
                    count--;
                }
                return str;
            }
        };

        /// <summary>
        /// 用于解析一般格式的 body，总使用 xml 格式
        /// 
        /// 
        ///         <?xml version='1.0' encoding='utf-8' ?>
        ///         <zonekey app='record_livingcast_service'>
        ///             <cmd command=xxxx>          // 此处 xxxx 为 record_start, record_stop, livingcast_start, livingcast_stop 四种
        ///                 <...>                     // 每个 command，此处 ....
        ///             </cmd>
        ///             <mcu>
        ///                 <jid>xxxx</jid>         // 此处 xxxx 为需要访问的 mcu 的完整 jid
        ///                 <cid>xxx</cid>          // 对应的 mcu 中的 cid
        ///             </mcu>
        ///         </zonekey>
        /// 
        /// 
        /// </summary>
        class BodyParser
        {
            /// <summary>
            /// 从 body 字符串中解析，成功返回 true，否则 false
            /// </summary>
            /// <param name="str">body字符串</param>
            /// <param name="command">出参，对应 command 属性</param>
            /// <param name="cmd_node">出参，对应整个 cmd 节点</param>
            /// <param name="mcu_jid">出参，mcu jid</param>
            /// <param name="mcu_cid">出参，mcu cid</param>
            /// <returns></returns>
            public static bool parse(string str, out string command, out XmlElement cmd_node, out string mcu_jid, out int mcu_cid)
            {
                command = null;
                cmd_node = null;
                mcu_cid = 0;
                mcu_jid = null;

                try
                {
                    XmlDocument doc = new XmlDocument();
                    doc.LoadXml(str);

                    XmlElement node_zonekey = doc.DocumentElement;
                    if (node_zonekey.Name == "zonekey")
                    {
                        XmlNode node_app = node_zonekey.Attributes.GetNamedItem("app");
                        if (node_app != null && node_app.NodeType == XmlNodeType.Attribute && ((XmlAttribute)node_app).Value == "record_livingcast_service")
                        {
                            // 找到 cmd 节点，和 mcu 节点
                            XmlNodeList cmds = doc.SelectNodes("/zonekey/cmd");
                            XmlNodeList mcus = doc.SelectNodes("/zonekey/mcu");

                            if (cmds.Count >= 1 && mcus.Count >= 1) 
                            {
                                cmd_node = (XmlElement)cmds[0];
                                XmlElement mcu_node = (XmlElement)mcus[0];

                                XmlNode attr_command = cmd_node.Attributes.GetNamedItem("command");
                                if (attr_command != null)
                                {
                                    command = attr_command.Value;

                                    int state = 0;
                                    foreach (XmlNode cn in mcu_node.ChildNodes)
                                    {
                                        if (cn.Name == "jid")
                                        {
                                            mcu_jid = cn.InnerText;
                                            state |= 1;
                                        }
                                        else if (cn.Name == "cid")
                                        {
                                            mcu_cid = int.Parse(cn.InnerText);
                                            state |= 2;
                                        }
                                    }

                                    return state == 3;  // 拥有 mcu_jid, mcu_cid
                                }
                            }
                        }
                    }

                    return false;
                }
                catch
                {
                    return false;
                }
            }
        };

        class Mse
        {
            public static string xmpp_domain()
            {
                string domain = Environment.GetEnvironmentVariable("xmpp_domain");
                if (domain == null)
                {
                    throw new Exception("必须设置系统环境变量：xmpp_domain");
                }
                return domain;
            }

            // 获取首选 mac 地址
            static string mac_ = null;
            public static string getmac()
            {
                if (mac_ == null)
                {
                    int ver = Environment.OSVersion.Version.Major;
                    string str = "Select MACAddress,PNPDeviceID,NetConnectionStatus,Availability,NetEnabled FROM Win32_NetworkAdapter WHERE MACAddress IS NOT NULL AND PNPDeviceID IS NOT NULL";
                    if (ver < 6)
                        str = "Select MACAddress,PNPDeviceID,Availability FROM Win32_NetworkAdapter WHERE MACAddress IS NOT NULL AND PNPDeviceID IS NOT NULL";

                    ManagementObjectSearcher searcher = new ManagementObjectSearcher(str);
                    ManagementObjectCollection mObject = searcher.Get();
                    mac_ = "000000000000";

                    foreach (ManagementObject obj in mObject)
                    {
                        string pnp = obj["PNPDeviceID"].ToString();
                        UInt16 ss = (UInt16)(obj["Availability"]);
                        Boolean enabled = true;
                        UInt16 status = 2;
                        if (ver >= 6)
                        {
                            try
                            {
                                enabled = (Boolean)(obj["NetEnabled"]);
                            }
                            catch { }
                            try
                            {
                                status = (UInt16)(obj["NetConnectionStatus"]);
                            }
                            catch { }
                        }
                        int a = status;
                        if (pnp.Contains("PCI\\") && status == 2 && ss == 3 && enabled)
                        {
                            string mac = obj["MACAddress"].ToString();
                            mac = mac.Replace(":", string.Empty);
                            mac_ = mac.ToLower();
                            break;
                        }
                    }
                }

                return mac_;
            }

            // 获取首选 ip
            static string ip_ = null;
            public static string getip()
            {
                if (ip_ == null)
                {
                    string mac = getmac();
                    foreach (NetworkInterface ni in NetworkInterface.GetAllNetworkInterfaces())
                    {
                        string macaddr = ni.GetPhysicalAddress().ToString().ToLower();
                        macaddr = macaddr.Replace(":", string.Empty);
                        if (macaddr == mac)
                        {
                            foreach (UnicastIPAddressInformation ip in ni.GetIPProperties().UnicastAddresses)
                            {
                                if (ip.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
                                {
                                    ip_ = ip.Address.ToString();
                                    break;
                                }
                            }

                            break;
                        }
                    }
                }

                return ip_;
            }
        };
	}
}
