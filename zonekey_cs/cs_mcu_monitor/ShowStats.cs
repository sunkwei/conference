using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cs_mcu_monitor
{
    /// <summary>
    /// 声明显示信息的接口
    /// </summary>
    interface ShowStats
    {
        /// <summary>
        /// 更新信息，
        /// </summary>
        /// <param name="new_conference_stat">最新的统计信息</param>
        /// <param name="old_conference_stat">上次的统信息</param>
        /// <param name="delta">两次统计信息之的时间差，秒</param>
        void UpdateConference(Conference new_conference_stat, Conference old_conference_stat, double delta);
    }
}
