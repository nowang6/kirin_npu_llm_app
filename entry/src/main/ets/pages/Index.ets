import { promptAction } from '@kit.ArkUI';
import testNapi from 'libentry.so';
import { hilog } from '@kit.PerformanceAnalysisKit';

interface Input {
  prompt : string;
}

interface Output {
  text : string;
}

@ObservedV2
class messageType {
  role: string
  @Trace context: string

  constructor() {
    this.role = 'user'
    this.context = ''
  }
}

@Entry
@Component
export struct MainPage{
  bgc: string = '#ffffffff'
  TextAreaController: TextAreaController = new TextAreaController()
  listScroller: Scroller = new Scroller()

  @State messages: messageType[] = [
    {role: 'assistant', context: testNapi.loadmodel() },
  ]

  @State inputValue: string = ''

  @State answerText: string = "";

  sendMessage() {
    if (this.inputValue.trim() === '') {
      promptAction.showToast({
        message: '请输入内容',
        duration: 1500
      })
      return
    }
    this.answerText = ""
    let toSend = this.inputValue
    this.messages.push({role: 'user', context: toSend})
    this.inputValue = ""
    this.listScroller.scrollEdge(Edge.Bottom)
    this.replyMessage(toSend)
  }

  private registerNativeCallback(){
    testNapi.answerget(
      (token: string) => {
        this.onTokenReceived(token);
      }
    );
  }

  onTokenReceived(token: string) {
    Promise.resolve().then(() => {
      this.answerText += token;
      this.messages[this.messages.length - 1] = {
        role: 'assistant',
        context: this.answerText
      }
      this.listScroller.scrollEdge(Edge.Bottom)
    });
  }

  async replyMessage(userPrompt: string) {
    this.messages.push({role: 'assistant', context: "Waiting for response......."})
    this.registerNativeCallback();
    testNapi.modelinfer(userPrompt)
  }

  build() {
    Column(){
      Text("CANN LLM")
        .fontSize(30)
        .fontWeight(FontWeight.Bold)
        .width('90%')
        .height(70)
        .backgroundColor('#ffffffff')
        .padding({ top: 10, bottom: 10 })
        .textAlign(TextAlign.Start)
      Column() {
        List({
          space: 20,
          initialIndex: this.messages.length - 1,
          scroller: this.listScroller
        }) {
          ForEach(this.messages, (message: messageType, index: number) => {
            ListItem() {
              Row() {
                Row() {
                  Text(message.context)
                    .backgroundColor(message.role === 'user' ? '#ff33c4fd' : '#b49d9d9d')
                    .fontSize(15)
                    .fontColor('#1C1B21')
                    .padding(10)
                    .textAlign(TextAlign.Start)
                    .borderRadius(10)
                }
                .justifyContent(message.role === 'user' ? FlexAlign.End : FlexAlign.Start)
                .layoutWeight(1)
              }
              .alignItems(VerticalAlign.Top)
              .width('100%')
            }
          })
        }
        .padding({
          top: 10,
          bottom: 10,
          left: 10,
          right: 10
        })
      }
      .backgroundColor(this.bgc)
      .layoutWeight(1)
      .width('100%')

      Row() {
        TextArea({
          placeholder: '请输入内容',
          text: $$this.inputValue,
          controller: this.TextAreaController
        })
          .height('100%')
          .margin(0)
          .width('70%')
          .fontSize(20)
          .lineHeight(30)
          .enterKeyType(EnterKeyType.Send)
          .backgroundColor(Color.White)
          .borderColor(Color.Black)
          .borderWidth(1)
          .textAlign(TextAlign.Center)

        Button("发送")
          .width('25%')
          .height('100%')
          .backgroundColor(Color.White)
          .borderWidth(1)
          .borderColor(Color.Black)
          .fontColor(Color.Black)
          .onClick(() => {
            this.sendMessage()
          })
      }
      .width('90%')
      .height(45)
      .alignItems(VerticalAlign.Bottom)
      .justifyContent(FlexAlign.SpaceBetween)
      .borderRadius(10)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.bgc)
  }

}

