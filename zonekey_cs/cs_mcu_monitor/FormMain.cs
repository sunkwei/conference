using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using Newtonsoft.Json.Linq;

namespace cs_mcu_monitor
{
    public partial class FormMain : Form
    {
        public FormMain()
        {
            InitializeComponent();
        }

        delegate void GuiDelegateAfterListConferences();
        void afterListConference()
        {
            if (CurrentCids == null) {
                tabControl1.TabPages.Clear();   // 删除所有
                return;
            }

            List<TabPage> pages = new List<TabPage>();
            foreach (TabPage page in tabControl1.TabPages) pages.Add(page);

            // 检查是否需要移除 pages
            foreach (TabPage page in pages) {
                int cid = (int)page.Tag;
                if (!CurrentCids.Contains(cid))
                    tabControl1.TabPages.Remove(page);
            }

            // 检查是否需要删除/添加 tabpage
            foreach (int cid in CurrentCids) {
                bool found = false;
                foreach (TabPage page in tabControl1.TabPages) {
                    int id = (int)page.Tag;
                    if (cid == id) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // 新的 cid
                    TabPage page = makePageToShowConference(cid);
                    tabControl1.TabPages.Add(page);
                }
            }
        }

        /// <summary>
        /// 构造一个显示页面，每个页面必须实现 ShowConference 接口
        /// </summary>
        /// <param name="cid"></param>
        /// <returns></returns>
        TabPage makePageToShowConference(int cid)
        {
            ShowConference_1 page = new ShowConference_1();
            page.Text = string.Format("{0}", cid);
            page.Tag = cid;
            return page;
        }

        List<int> CurrentCids { get; set; }

        void list_conferences_result(List<int> cids)
        {
            CurrentCids = cids;
            Invoke(new GuiDelegateAfterListConferences(afterListConference));

            // 针对每个 cids 发出 info ...
            foreach (int cid in cids) {
                UAC.Instance().InfoConference(cid, new UAC.delegate_info_conference_result(info_conference_result));
            }
        }

        Conference saved_last_conference_ = null;
        DateTime saved_last_conference_stamp_;

        delegate void GuiDelegateAfterInfoConference(int cid, Conference conference);
        void afterInfoConference(int cid, Conference conference)
        {
            DateTime now = DateTime.Now;
            if (saved_last_conference_ == null) {
                saved_last_conference_stamp_ = now;
                saved_last_conference_ = conference;
            }

            TimeSpan x = now.Subtract(saved_last_conference_stamp_);
            double delta = x.TotalSeconds;

            // 找到 cid 对应的 page，然后更新数据
            foreach (TabPage page in tabControl1.TabPages) {
                if (cid == (int)page.Tag) {
                    ((ShowStats)page).UpdateConference(conference, saved_last_conference_, delta);
                    break;
                }
            }

            saved_last_conference_ = conference;
            saved_last_conference_stamp_ = now;
        }

        void info_conference_result(string str_json)
        {
            Conference conference = new Conference(str_json);
            Invoke(new GuiDelegateAfterInfoConference(afterInfoConference), new object[] { conference.cid, conference });
        }

        private void FormMain_Load(object sender, EventArgs e)
        {
            //UAC.Instance().ListConferences(list_conferences_result);
            tabControl1.TabPages.Clear();
            timer1.Start();
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            UAC.Instance().ListConferences(list_conferences_result);
        }
    }
}
