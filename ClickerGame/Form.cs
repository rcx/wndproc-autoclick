using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ClickerGame
{
    public partial class Form : System.Windows.Forms.Form
    {
        int Clicks;
        int TotalClicks;
        bool Started = false;
        DateTime TimeStart;

        public Form()
        {
            InitializeComponent();
        }

        private void button_Click(object sender, EventArgs e)
        {
            Clicks++;
            Started = true;
            TimeStart = DateTime.Now;
            button.Click -= button_Click;
            button.Click += button_Click_Fast;
        }

        private void button_Click_Fast(object sender, EventArgs e)
        {
            Clicks++;
        }

        private void timer_Tick(object sender, EventArgs e)
        {
            TotalClicks += Clicks;
            float cps = Clicks / (timer.Interval / 1000.0f);
            float elapsed = ((DateTime.Now.Subtract(TimeStart)).Seconds);
            float cpsAvg = TotalClicks / elapsed;
            label.Text = cps.ToString("0.0") + " clicks per second, " + TotalClicks.ToString() + " total, " + cpsAvg.ToString("0.0") + " average";
            timeLabel.Text = elapsed.ToString("0.000");
            speedBar.Maximum = (int) Math.Max(100, cps);
            speedBar.Value = (int) cps;
            Clicks = 0;
        }

        private void Form_Load(object sender, EventArgs e)
        {
            label2.Text = "HWND = " + button.Handle.ToString("x8");
        }

        private void buttonReset_Click(object sender, EventArgs e)
        {
            TotalClicks = 0;
            Clicks = 0;
            Started = false;
            button.Click -= button_Click_Fast;
            button.Click += button_Click;
        }
    }
}
