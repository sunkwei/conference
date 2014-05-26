using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace cs_zkRecordingLivingCastServer
{
    class KVConfig : SortedDictionary<string, string>
    {
        public KVConfig(string filename)
        {
            try
            {
                StreamReader sr = new StreamReader(filename);
                string line = sr.ReadLine();
                while (line != null)
                {
                    line = line.Trim();
                    if (line.Length > 3 && line[0] != '#')
                    {

                        string[] kv = line.Split(new char[] { '=' }, 2);
                        if (kv.Length == 2)
                        {
                            this.Add(kv[0], kv[1]);
                        }
                    }

                    line = sr.ReadLine();
                }
                sr.Close();
            }
            catch { }
        }
    }
}
