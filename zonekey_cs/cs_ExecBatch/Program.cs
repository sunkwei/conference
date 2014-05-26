using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;

namespace cs_ExecBatch
{
    /// <summary>
    /// 为了方便用于安装包，执行一个批处理脚本文件
    /// </summary>
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length < 1) {
                Console.WriteLine("ERR: usage cs_ExecBatch.exe <batch filename> [params ...]");
                return;
            }

            string img_filename = Process.GetCurrentProcess().MainModule.FileName;
            int pos = img_filename.LastIndexOf('\\');
            if (pos > 0) {
                string path = img_filename.Substring(0, pos);

                // FIXME: 这里要求 第一个参数必须不是绝对路径 ....
                string batch_filename = path + "\\" + args[0];

                Process process = new Process();
                process.StartInfo.FileName = batch_filename;
                if (args.Length > 1) {
                    process.StartInfo.Arguments = "\"" + args[1] + "\"";
                    for (int i = 2; i < args.Length; i++) {
                        process.StartInfo.Arguments += " \"" + args[i] + "\"";
                    }
                }

                if (process.Start()) {
                    process.WaitForExit();  // 等待结束
                }
            }
        }
    }
}
