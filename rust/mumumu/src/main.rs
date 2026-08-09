use std::thread;
use std::time::Duration;
fn workout(intensity:u32,rand_number:u32)
{
    let action=||{
        println!("mumumu");
        thread::sleep(Duration::from_secs(2));
        intensity
    };
    if intensity<25{
        println!("今天活力满满，先做{}个俯卧撑！",action());
        println!("旁边有妹子在看,俯卧撑太low,再来{}组卧推",action());
    }
    else if rand_number==3 {
        println!("昨天练过度了,今天还是休息吧~");
    }else {
        println!("昨天练过度了,今天开始干有氧!先跑{}分钟",action());
    }
}
fn main(){
    let intensity=10;
    let rand_number=7;
    workout(intensity, rand_number);
}