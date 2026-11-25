use crate::drivers::can::CanLink;
use crate::runtime::shell::can_shell::CanShell;

#[embassy_executor::task]
pub async fn can_shell_task(link: &'static CanLink) {
    let shell = CanShell::new(link);
    loop {
        if let Ok(envelope) = link.receive().await {
            shell.handle_frame(&envelope.frame).await;
        }
    }
}
