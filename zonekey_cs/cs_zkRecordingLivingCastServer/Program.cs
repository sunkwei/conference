using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Net;
using System.Threading;

namespace cs_zkRecordingLivingCastServer
{
    class Program
    {
        static void Main(string[] args)
        {
            server_ = new Server();
            while (true)
                Thread.Sleep(1000);
            server_.Close();
            Console.WriteLine("end!");
        }

        static Server server_ = null;
        static ManualResetEvent evt_ = new ManualResetEvent(false);

        static void Console_CancelKeyPress(object sender, ConsoleCancelEventArgs e)
        {
            Console.WriteLine("ctrl+c pressed!");
            evt_.Set();
        }
    }
}
