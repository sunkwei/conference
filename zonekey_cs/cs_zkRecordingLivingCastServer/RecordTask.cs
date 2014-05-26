using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using System.Xml;
using System.Runtime.InteropServices;

namespace cs_zkRecordingLivingCastServer
{
    /// <summary>
    /// 描述一个完整的录像任务，对应一个 uuid（子目录），对应一个 meta.xml 文件，任务完成后，将创建一个计划任务 ....
    /// </summary>
    class RecordTask
    {
        const string config_filename_ = @"c:\zonekey.config\RecordingLivingcastServer\task.conf";
        KVConfig config = new KVConfig(config_filename_);

        // 创建录像目录，启动工作线程，可能抛出异常！
        public RecordTask(RecordStartParam param)
        {
            Param = param;

            // 录像存储目录，应该通过外部设置获取
            string record_path = ".\\";
            if (config.ContainsKey("record_path"))
                record_path = config["record_path"];

            BatchFile = @"c:\zonekey.config\RecordingLivingcastServer\ts_upload.bat";
            if (config.ContainsKey("task_script"))
                BatchFile = config["task_script"];

            DirectoryInfo di = new DirectoryInfo(record_path); // 
            RecordingPath = di.FullName + "\\";
            TaskPath = RecordingPath + Param.uuid;
            ScriptFilename = RecordingPath + Param.uuid + ".lftp";
            string cpath = winpath_2_cygpath(ScriptFilename);

            Directory.CreateDirectory(TaskPath);

            Quit = false;
            th = new Thread(new ThreadStart(proc_run));
            th.Start();
        }

        /// <summary>
        /// 停止录像，创建一个计划任务，在计划任务中，将录像上传 ....
        /// </summary>
        public void Close()
        {
            if (th != null)
            {
                Quit = true;
                th.Join();
            }
        }

        /// <summary>
        /// 录像工作线程
        /// </summary>
        void proc_run()
        {
            make_meta_file();

            List<RecordFile> recordings = new List<RecordFile>();
            foreach (RecordFileDesc fdesc in Param.files)
                recordings.Add(new RecordFile(Param.mcu_jid, TaskPath + "\\" + fdesc.filename, Param.mcu_cid, fdesc.aid, fdesc.vid));

            DateTime dt = DateTime.Now;

            while (!Quit)
                Thread.Sleep(100);  // 等待结束

            foreach (RecordFile rf in recordings)
                rf.Close();

            TimeSpan ts = DateTime.Now.Subtract(dt);
            make_local_playback_metafile(ts);
            cp_playback();
            make_sched_upload();
        }

        void make_meta_file()
        {
            // 生成符合纳博平台的 meta.xml 文件
            XmlDocument doc = new XmlDocument();
            doc.LoadXml("<?xml version='1.0' encoding='utf-8'?><Package><Files></Files></Package>");
            XmlElement package = doc.DocumentElement;

            XmlAttribute attr = doc.CreateAttribute("From");
            attr.Value = NaboConfig.Instance().DeviceID;
            package.Attributes.Append(attr);

            attr = doc.CreateAttribute("To");
            attr.Value = NaboConfig.Instance().Teacher + '@' + NaboConfig.Instance().Domain;
            package.Attributes.Append(attr);

            attr = doc.CreateAttribute("ID");
            attr.Value = Param.uuid;
            package.Attributes.Append(attr);

            attr = doc.CreateAttribute("Subject");
            attr.Value = Param.subject;
            package.Attributes.Append(attr);

            attr = doc.CreateAttribute("Title");
            attr.Value = NaboConfig.Instance().Title;
            package.Attributes.Append(attr);

            attr = doc.CreateAttribute("Grade");
            attr.Value = NaboConfig.Instance().Grade;
            package.Attributes.Append(attr);

            attr = doc.CreateAttribute("Teacher");
            attr.Value = NaboConfig.Instance().Teacher;
            package.Attributes.Append(attr);

            XmlNode files = doc.SelectSingleNode("/Package/Files");
            meta_add_media_files(files);

            string meta_file_name = TaskPath + "\\meta.xml.tmp";    // XXX：上传后，再修改文件名字！
            StreamWriter sw = new StreamWriter(meta_file_name);
            sw.Write(doc.OuterXml);
            sw.Close();
        }

        void meta_add_media_files(XmlNode files)
        {
            foreach (RecordFileDesc desc in Param.files)
            {
                XmlElement file = files.OwnerDocument.CreateElement("File");
                files.AppendChild(file);

                XmlAttribute attr = files.OwnerDocument.CreateAttribute("Caption");
                attr.Value = desc.desc;
                file.Attributes.Append(attr);

                attr = files.OwnerDocument.CreateAttribute("Format");
                attr.Value = "video/flv";
                file.Attributes.Append(attr);

                attr = files.OwnerDocument.CreateAttribute("Type");
                if (desc.vid == -1)
                    attr.Value = "movie";
                else
                    attr.Value = "resource";
                file.Attributes.Append(attr);

                attr = files.OwnerDocument.CreateAttribute("File");
                attr.Value = desc.filename;
                file.Attributes.Append(attr);
            }
        }

        void cp_playback()
        {
            string cmd = "copy \"C:\\zonekey.config\\RecordingLivingcastServer\\*.exe\" \"" + TaskPath + "\"";
            WinExec(cmd, 0);
        }

        void make_local_playback_metafile(TimeSpan ts)
        {
            try {
                string filename = TaskPath + "\\flvmeta.txt";
                StreamWriter sw = new StreamWriter(filename);

                sw.WriteLine("name=" + "interact");
                sw.WriteLine("teacher=" + "");
                sw.WriteLine("course=" + "interact");
                sw.WriteLine("date=" + "");

                int n = 1;
                foreach (RecordFileDesc desc in Param.files) {
                    string key = "file" + n.ToString();
                    sw.WriteLine(key + '=' + desc.filename);
                    n++;
                }
                sw.WriteLine("playlength=" + ts.TotalSeconds.ToString());

                sw.Close();
            }
            catch { }
        }

        string conv_df(string fp)
        {
            string cygpath = "/cygdrive/";
            cygpath += fp[0];
            for (int i = 2; i < fp.Length; i++)
            {
                // FIXME：可能还需要考虑汉字问题
                switch (fp[i])
                {
                    case '\\':
                        cygpath += '/';
                        break;

                    case ' ':
                        cygpath += "\\ ";
                        break;

                    default:
                        cygpath += fp[i];
                        break;
                }
            }
            return cygpath;
        }

        /// <summary>
        /// 将 winpath c:\test\a 转换为 cygin 格式：/cygdrive/c/test/a
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        string winpath_2_cygpath(string path)
        {
            FileInfo di = new FileInfo(path);
            if (di.Exists)
            {
                string fp = di.FullName;    // c:\test\subdir
                return conv_df(fp);
            }
            else
            {
                // 可能是目录
                DirectoryInfo i = new DirectoryInfo(path);
                if (i.Exists)
                    return conv_df(i.FullName);

                return "./";    // 呵呵，防止出错
            }
        }

        /// <summary>
        /// 生成 lftp 使用的上传脚本，该脚本将在计划任务中执行
        /// </summary>
        string make_lftp_script()
        {
            // 使用 uuid.lftp 作为文件名字
            DirectoryInfo di = new DirectoryInfo(RecordingPath);
            StreamWriter sw = new StreamWriter(ScriptFilename);
            sw.WriteLine("set ftp:charset gb2312");
            sw.WriteLine("set file:charset gb2312");
            sw.WriteLine("set ftp:passive-mode off");
            //sw.WriteLine("set net:connection-limit 1");
            //sw.WriteLine("set net:limit-total-rate 100000 100000");
            sw.WriteLine("set net:max-retries 2");
            sw.WriteLine("set net:reconnect-interval-base 2");
            sw.WriteLine("set net:reconnect-interval-max 5");
            sw.WriteLine("set mirror:set-permissions off");
            sw.WriteLine("connect " + NaboConfig.Instance().FtpPath);
            sw.WriteLine("mirror -R " + winpath_2_cygpath(TaskPath));
            sw.WriteLine("mv " + Param.uuid + "/meta.xml.tmp " + Param.uuid + "/meta.xml"); // XXX: 最后的文件名字
            sw.Close();

            return ScriptFilename;
        }

        void make_sched_upload()
        {
            string script_full_path = make_lftp_script();

            /** 批处理文件，在计划任务中执行
@echo off
rem xxxx <ts name> <lftp script file> <record task path> <patch file>

rem 使用 lftp 上传
lftp -f %2

IF %errorlevel% NEQ 0 GOTO err

rem 此时上传完成，删除计划任务，删除目录
rm -fr %3
schtasks /delete /tn %1 /f
goto end

:err
rem 错误，此时需要删除老的计划任务，建新的
schtasks /delete /tn %1 /f

rem 计算下次计划任务的时间，5到10分钟吧
set /a t1=%random%%%5+5
set t2=%time:~0,-9%
set t3=:
set /a t4=%t2%+1
set /a t4=%t4% %% 24
set t=%t4%%t3%%t1%

schtasks /create /sc once /tn %1 /tr "%0 %1 %2 %3" /st %t%

:end
@echo on
             */

            //  创建 windows 计划任务，上传 ...
            DateTime when = DateTime.Now.AddMinutes(1); //1分钟之后的计划
            string st = when.ToString("HH:mm");
            string bat = BatchFile;
            //string cmdline = "schtasks /create /sc once /tn " + "zk_" + Param.uuid +
            //    " /tr \"" + bat + " " + "zk_" + Param.uuid + " " + winpath_2_cygpath(ScriptFilename) + " " + TaskPath + "\"" + 
            //    " /st " + st;

            string torun = "\"" + bat + " " + "zk_" + Param.uuid + " " + winpath_2_cygpath(ScriptFilename) + " " + TaskPath + "\"";
            string startbat = @"c:\zonekey.config\RecordingLivingcastServer\stscht.bat";
            if (config.ContainsKey("start_script"))
                startbat = config["start_script"];
            string cmd = startbat + " zk_" + Param.uuid + " " + torun + " " + st;

            WinExec(cmd, 0);
        }

        [DllImport("kernel32.dll")]
        public static extern int WinExec(string exeName, int operType);

        int TaskDuration { get; set; }
        Thread th { get; set; }
        RecordStartParam Param { get; set; }
        bool Quit { get; set; }
        string TaskPath { get; set; }
        string ScriptFilename { get; set; } // lftp 脚本的完整文件名
        string RecordingPath { get; set;}
        string BatchFile { get; set; }
    }
}
