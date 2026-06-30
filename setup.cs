using System;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Installer {
    public class SetupForm : Form {
        private Label lblStatus;
        private ProgressBar progressBar;
        
        public SetupForm() {
            this.Text = "WallpaperAnim Kurulum";
            this.Size = new Size(400, 180);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            
            Label lblTitle = new Label();
            lblTitle.Text = "WallpaperAnim Yükleniyor...";
            lblTitle.Font = new Font("Segoe UI", 12, FontStyle.Bold);
            lblTitle.Location = new Point(20, 20);
            lblTitle.AutoSize = true;
            this.Controls.Add(lblTitle);
            
            lblStatus = new Label();
            lblStatus.Text = "Lütfen bekleyin, dosyalar çıkartılıyor...";
            lblStatus.Location = new Point(20, 55);
            lblStatus.AutoSize = true;
            this.Controls.Add(lblStatus);
            
            progressBar = new ProgressBar();
            progressBar.Style = ProgressBarStyle.Marquee;
            progressBar.Location = new Point(20, 80);
            progressBar.Size = new Size(340, 25);
            this.Controls.Add(progressBar);
        }
        
        protected override async void OnShown(EventArgs e) {
            base.OnShown(e);
            
            await Task.Run(() => {
                try {
                    string targetDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "WallpaperAnim");
                    
                    if (Directory.Exists(targetDir)) {
                        UpdateStatus("Eski sürüm kapatılıyor...");
                        foreach (var process in System.Diagnostics.Process.GetProcessesByName("WallpaperAnimWinUI")) {
                            process.Kill();
                        }
                        System.Threading.Thread.Sleep(1000);
                        Directory.Delete(targetDir, true);
                    }
                    Directory.CreateDirectory(targetDir);

                    UpdateStatus("Dosyalar çıkartılıyor (Bu işlem birkaç saniye sürebilir)...");
                    using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("app.zip"))
                    using (ZipArchive archive = new ZipArchive(stream)) {
                        archive.ExtractToDirectory(targetDir);
                    }
                    
                    UpdateStatus("Kısayollar oluşturuluyor...");
                    string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                    string shortcutLoc = Path.Combine(desktopPath, "WallpaperAnim.lnk");
                    CreateShortcut(shortcutLoc, Path.Combine(targetDir, "WallpaperAnimWinUI.exe"), targetDir);

                    string startMenu = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "WallpaperAnim.lnk");
                    CreateShortcut(startMenu, Path.Combine(targetDir, "WallpaperAnimWinUI.exe"), targetDir);

                    // Launch app
                    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo() {
                        FileName = Path.Combine(targetDir, "WallpaperAnimWinUI.exe"),
                        WorkingDirectory = targetDir
                    });
                    
                    this.Invoke((MethodInvoker)delegate {
                        MessageBox.Show(this, "Kurulum başarıyla tamamlandı!", "Başarılı", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        this.Close();
                    });
                } catch(Exception ex) {
                    this.Invoke((MethodInvoker)delegate {
                        MessageBox.Show(this, "Kurulum sırasında hata oluştu:\n" + ex.Message, "Hata", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        this.Close();
                    });
                }
            });
        }
        
        private void UpdateStatus(string text) {
            this.Invoke((MethodInvoker)delegate {
                lblStatus.Text = text;
            });
        }
        
        static void CreateShortcut(string shortcutPath, string targetPath, string workingDir) {
            Type t = Type.GetTypeFromProgID("WScript.Shell");
            dynamic shell = Activator.CreateInstance(t);
            var shortcut = shell.CreateShortcut(shortcutPath);
            shortcut.TargetPath = targetPath;
            shortcut.WorkingDirectory = workingDir;
            shortcut.Save();
        }
    }

    static class Program {
        [STAThread]
        static void Main() {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            
            var res = MessageBox.Show("WallpaperAnim uygulamasını bilgisayarınıza yüklemek istiyor musunuz?", "WallpaperAnim Kurulum", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
            if(res == DialogResult.Yes) {
                Application.Run(new SetupForm());
            }
        }
    }
}
