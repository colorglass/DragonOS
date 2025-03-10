use alloc::{
    collections::BTreeMap,
    string::{String, ToString},
    sync::{Arc, Weak},
};
use system_error::SystemError;
use unified_init::macros::unified_init;

use crate::{
    filesystem::{
        devfs::{devfs_register, DevFS, DeviceINode},
        vfs::{
            file::FileMode, syscall::ModeType, FilePrivateData, FileType, IndexNode, Metadata,
            ROOT_INODE,
        },
    },
    init::initcall::INITCALL_DEVICE,
    kerror,
    libs::{
        lib_ui::textui::{textui_putchar, FontColor},
        rwlock::RwLock,
    },
};

use super::{serial::serial_init, TtyCore, TtyError, TtyFileFlag, TtyFilePrivateData};

lazy_static! {
    /// 所有TTY设备的B树。用于根据名字，找到Arc<TtyDevice>
    /// TODO: 待设备驱动模型完善，具有类似功能的机制后，删掉这里
    pub static ref TTY_DEVICES: RwLock<BTreeMap<String, Arc<TtyDevice>>> = RwLock::new(BTreeMap::new());
}

/// @brief TTY设备
#[derive(Debug)]
pub struct TtyDevice {
    /// TTY核心
    core: TtyCore,
    /// TTY所属的文件系统
    fs: RwLock<Weak<DevFS>>,
    /// TTY设备私有信息
    private_data: RwLock<TtyDevicePrivateData>,
}

#[derive(Debug)]
struct TtyDevicePrivateData {
    /// TTY设备名(如tty1)
    name: String,
    /// TTY设备文件的元数据
    metadata: Metadata,
    // TODO: 增加指向输出端口连接的设备的指针
}

impl TtyDevice {
    pub fn new(name: &str) -> Arc<TtyDevice> {
        let result = Arc::new(TtyDevice {
            core: TtyCore::new(),
            fs: RwLock::new(Weak::default()),
            private_data: TtyDevicePrivateData::new(name),
        });
        // 默认开启输入回显
        result.core.enable_echo();
        return result;
    }

    /// @brief 判断文件私有信息是否为TTY文件的私有信息
    #[inline]
    fn verify_file_private_data<'a>(
        &self,
        private_data: &'a mut FilePrivateData,
    ) -> Result<&'a mut TtyFilePrivateData, SystemError> {
        if let FilePrivateData::Tty(t) = private_data {
            return Ok(t);
        }
        return Err(SystemError::EIO);
    }

    /// @brief 获取TTY设备名
    #[inline]
    pub fn name(&self) -> String {
        return self.private_data.read().name.clone();
    }

    /// @brief 检查TTY文件的读写参数是否合法
    #[inline]
    pub fn check_rw_param(&self, len: usize, buf: &[u8]) -> Result<(), SystemError> {
        if len > buf.len() {
            return Err(SystemError::EINVAL);
        }
        return Ok(());
    }

    /// @brief 向TTY的输入端口导入数据
    pub fn input(&self, buf: &[u8]) -> Result<usize, SystemError> {
        let r: Result<usize, TtyError> = self.core.input(buf, false);
        if r.is_ok() {
            return Ok(r.unwrap());
        }

        let r = r.unwrap_err();
        match r {
            TtyError::BufferFull(x) => return Ok(x),
            TtyError::Closed => return Err(SystemError::ENODEV),
            e => {
                kerror!("tty error occurred while writing data to its input port, msg={e:?}");
                return Err(SystemError::EBUSY);
            }
        }
    }
}

impl DeviceINode for TtyDevice {
    fn set_fs(&self, fs: alloc::sync::Weak<crate::filesystem::devfs::DevFS>) {
        *self.fs.write() = fs;
    }
}

impl IndexNode for TtyDevice {
    /// @brief 打开TTY设备
    ///
    /// @param data 文件私有信息
    /// @param mode 打开模式
    ///
    /// TTY设备通过mode来确定这个文件到底是stdin/stdout/stderr
    /// - mode的值为O_RDONLY时，表示这个文件是stdin
    /// - mode的值为O_WRONLY时，表示这个文件是stdout
    /// - mode的值为O_WRONLY | O_SYNC时，表示这个文件是stderr
    fn open(&self, data: &mut FilePrivateData, mode: &FileMode) -> Result<(), SystemError> {
        let mut p = TtyFilePrivateData::default();

        // 检查打开模式
        let accmode = mode.accmode();
        if accmode == FileMode::O_RDONLY.accmode() {
            p.flags.insert(TtyFileFlag::STDIN);
        } else if accmode == FileMode::O_WRONLY.accmode() {
            if mode.contains(FileMode::O_SYNC) {
                p.flags.insert(TtyFileFlag::STDERR);
            } else {
                p.flags.insert(TtyFileFlag::STDOUT);
            }
        } else {
            return Err(SystemError::EINVAL);
        }

        // 保存文件私有信息
        *data = FilePrivateData::Tty(p);
        return Ok(());
    }

    fn read_at(
        &self,
        _offset: usize,
        len: usize,
        buf: &mut [u8],
        data: &mut crate::filesystem::vfs::FilePrivateData,
    ) -> Result<usize, SystemError> {
        let _data: &mut TtyFilePrivateData = match self.verify_file_private_data(data) {
            Ok(t) => t,
            Err(e) => {
                kerror!("Try to read tty device, but file private data type mismatch!");
                return Err(e);
            }
        };
        self.check_rw_param(len, buf)?;

        // 读取stdin队列
        let r: Result<usize, TtyError> = self.core.read_stdin(&mut buf[0..len], true);
        if r.is_ok() {
            return Ok(r.unwrap());
        }

        match r.unwrap_err() {
            TtyError::EOF(n) => {
                return Ok(n);
            }

            x => {
                kerror!("Error occurred when reading tty, msg={x:?}");
                return Err(SystemError::ECONNABORTED);
            }
        }
    }

    fn write_at(
        &self,
        _offset: usize,
        len: usize,
        buf: &[u8],
        data: &mut crate::filesystem::vfs::FilePrivateData,
    ) -> Result<usize, SystemError> {
        let data: &mut TtyFilePrivateData = match self.verify_file_private_data(data) {
            Ok(t) => t,
            Err(e) => {
                kerror!("Try to write tty device, but file private data type mismatch!");
                return Err(e);
            }
        };

        self.check_rw_param(len, buf)?;

        let mut cnt: usize = 0;
        // 根据当前文件是stdout还是stderr,选择不同的发送方式
        let r: Result<usize, TtyError> = if data.flags.contains(TtyFileFlag::STDOUT) {
            loop {
                let r = self.core.stdout(&buf[cnt..len], false);
                if let Err(TtyError::BufferFull(c)) = r {
                    self.sync().expect("Failed to sync tty device!");
                    cnt += c;
                } else {
                    break r;
                }
            }
        } else if data.flags.contains(TtyFileFlag::STDERR) {
            loop {
                let r = self.core.stderr(&buf[cnt..len], false);
                if let Err(TtyError::BufferFull(c)) = r {
                    self.sync().expect("Failed to sync tty device!");
                    cnt += c;
                } else {
                    break r;
                }
            }
        } else {
            return Err(SystemError::EPERM);
        };

        if r.is_ok() {
            self.sync().expect("Failed to sync tty device!");
            return Ok(cnt + r.unwrap());
        }

        let r: TtyError = r.unwrap_err();
        kerror!("Error occurred when writing tty deivce. Error msg={r:?}");
        return Err(SystemError::EIO);
    }

    fn fs(&self) -> Arc<dyn crate::filesystem::vfs::FileSystem> {
        return self.fs.read().upgrade().unwrap();
    }

    fn as_any_ref(&self) -> &dyn core::any::Any {
        self
    }

    fn list(&self) -> Result<alloc::vec::Vec<alloc::string::String>, SystemError> {
        return Err(SystemError::EOPNOTSUPP_OR_ENOTSUP);
    }

    fn metadata(&self) -> Result<Metadata, SystemError> {
        return Ok(self.private_data.read().metadata.clone());
    }

    fn close(&self, _data: &mut FilePrivateData) -> Result<(), SystemError> {
        return Ok(());
    }

    fn sync(&self) -> Result<(), SystemError> {
        // TODO: 引入IO重定向后，需要将输出重定向到对应的设备。
        // 目前只是简单的输出到屏幕（为了实现的简便）

        loop {
            let mut buf = [0u8; 512];
            let r: Result<usize, TtyError> = self.core.output(&mut buf[0..511], false);
            let len;
            match r {
                Ok(x) => {
                    len = x;
                }
                Err(TtyError::EOF(x)) | Err(TtyError::BufferEmpty(x)) => {
                    len = x;
                }
                _ => return Err(SystemError::EIO),
            }

            if len == 0 {
                break;
            }
            // 输出到屏幕

            for x in 0..len {
                textui_putchar(buf[x] as char, FontColor::WHITE, FontColor::BLACK).ok();
            }
        }
        return Ok(());
    }
    fn resize(&self, _len: usize) -> Result<(), SystemError> {
        return Ok(());
    }
}

impl TtyDevicePrivateData {
    pub fn new(name: &str) -> RwLock<Self> {
        let mut metadata = Metadata::new(FileType::CharDevice, ModeType::from_bits_truncate(0o755));
        metadata.size = TtyCore::STDIN_BUF_SIZE as i64;
        return RwLock::new(TtyDevicePrivateData {
            name: name.to_string(),
            metadata,
        });
    }
}

/// @brief 初始化TTY设备
#[unified_init(INITCALL_DEVICE)]
pub fn tty_init() -> Result<(), SystemError> {
    let tty: Arc<TtyDevice> = TtyDevice::new("tty0");
    let devfs_root_inode = ROOT_INODE().lookup("/dev");
    if devfs_root_inode.is_err() {
        return Err(devfs_root_inode.unwrap_err());
    }
    // 当前关闭键盘输入回显
    // TODO: 完善Termios之后, 改为默认开启键盘输入回显.
    tty.core.disable_echo();
    let guard = TTY_DEVICES.upgradeable_read();

    // 如果已经存在了这个设备
    if guard.contains_key("tty0") {
        return Err(SystemError::EEXIST);
    }

    let mut guard = guard.upgrade();

    guard.insert("tty0".to_string(), tty.clone());

    drop(guard);

    let r = devfs_register(&tty.name(), tty);
    if r.is_err() {
        return Err(devfs_root_inode.unwrap_err());
    }

    serial_init()?;
    return Ok(());
}
